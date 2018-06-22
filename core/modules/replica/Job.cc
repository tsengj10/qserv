/*
 * LSST Data Management System
 * Copyright 2017 LSST Corporation.
 *
 * This product includes software developed by the
 * LSST Project (http://www.lsst.org/).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the LSST License Statement and
 * the GNU General Public License along with this program.  If not,
 * see <http://www.lsstcorp.org/LegalNotices/>.
 */

// Class header
#include "replica/Job.h"

// System headers
#include <sstream>
#include <stdexcept>
#include <thread>
#include <utility>      // std::swap

// Third party headers
#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

// Qserv headers
#include "lsst/log/Log.h"
#include "replica/AddReplicaQservMgtRequest.h"
#include "replica/Common.h"            // Generators::uniqueId()
#include "replica/Configuration.h"
#include "replica/DatabaseServices.h"
#include "replica/Performance.h"       // PerformanceUtils::now()
#include "replica/QservMgtServices.h"
#include "replica/RemoveReplicaQservMgtRequest.h"
#include "replica/ServiceProvider.h"
#include "util/IterableFormatter.h"


namespace {

LOG_LOGGER _log = LOG_GET("lsst.qserv.replica.Job");

} /// namespace

namespace lsst {
namespace qserv {
namespace replica {

std::string Job::state2string(State state) {
    switch (state) {
        case CREATED:     return "CREATED";
        case IN_PROGRESS: return "IN_PROGRESS";
        case FINISHED:    return "FINISHED";
    }
    throw std::logic_error(
                "incomplete implementation of method Job::state2string(State)");
}

std::string
Job::state2string(ExtendedState state) {
    switch (state) {
        case NONE:               return "NONE";
        case SUCCESS:            return "SUCCESS";
        case CONFIG_ERROR:       return "CONFIG_ERROR";
        case FAILED:             return "FAILED";
        case QSERV_FAILED:       return "QSERV_FAILED";
        case QSERV_CHUNK_IN_USE: return "QSERV_CHUNK_IN_USE";
        case TIMEOUT_EXPIRED:    return "TIMEOUT_EXPIRED";
        case CANCELLED:          return "CANCELLED";
    }
    throw std::logic_error(
                "incomplete implementation of method Job::state2string(ExtendedState)");
}

Job::Job(Controller::Ptr const& controller,
         std::string const& parentJobId,
         std::string const& type,
         Options const& options)
    :   _id(Generators::uniqueId()),
        _controller(controller),
        _parentJobId(parentJobId),
        _type(type),
        _options(options),
        _state(State::CREATED),
        _extendedState(ExtendedState::NONE),
        _beginTime(0),
        _endTime(0),
        _heartbeatTimerIvalSec(controller->serviceProvider()->config()->jobHeartbeatTimeoutSec()),
        _expirationIvalSec(controller->serviceProvider()->config()->jobTimeoutSec()) {
}

std::string Job::state2string() const {
    util::Lock lock(_mtx, context() + "state2string");
    return state2string(state(), extendedState());
}

Job::Options Job::options() const {

    LOGS(_log, LOG_LVL_DEBUG, context() << "options");

    util::Lock lock(_mtx, context() + "options");

    return options(lock);
}

Job::Options Job::options(util::Lock const& lock) const {
    return _options;
}

Job::Options Job::setOptions(Options const& newOptions) {

    LOGS(_log, LOG_LVL_DEBUG, context() << "setOptions");

    util::Lock lock(_mtx, context() + "setOptions");

    Options options = newOptions;
    std::swap(_options, options);

    return options;
}

std::string Job::context() const {
    return  "JOB     " + id() + "  " + type() +
            "  " + state2string(state(), extendedState()) + "  ";
}

void Job::start() {

    LOGS(_log, LOG_LVL_DEBUG, context() << "start");

    util::Lock lock(_mtx, context() + "start");

    assertState(lock,
                State::CREATED,
                context() + "start");

    // IMPORTANT: update these before proceeding to the implementation
    // because the later may create children jobs whose performance
    // counters must be newer, and whose saved state within the database
    // may depend on this job's state.

    _beginTime = PerformanceUtils::now();
    controller()->serviceProvider()->databaseServices()->saveState(*this, options(lock));

    // Start timers if configured

    startHeartbeatTimer(lock);
    startExpirationTimer(lock);

    // Delegate the rest to the specific implementation

    startImpl(lock);

    // Allow the job to be fully accomplished right away

    if (state() == State::FINISHED) {
        notify();
        return;
    }

    // Otherwise, the only other state which is allowed here is this

    assertState(lock,
                State::IN_PROGRESS,
                context() + "start");
}

void Job::cancel() {

    LOGS(_log, LOG_LVL_DEBUG, context() << "cancel");

    // IMPORTANT: the final state is required to be tested twice. The first time
    // it's done in order to avoid deadlock on the "in-flight" requests reporting
    // their completion while the job termination is in a progress. And the second
    // test is made after acquering the lock to recheck the state in case if it
    // has transitioned while acquering the lock.

    if (state() == State::FINISHED) return;

    util::Lock lock(_mtx, context() + "cancel");

    if (state() == State::FINISHED) return;

    finish(lock, ExtendedState::CANCELLED);
}

void Job::finish(util::Lock const& lock,
                 ExtendedState newExtendedState) {

    LOGS(_log, LOG_LVL_DEBUG, context() << "finish"
         << "  newExtendedState=" << state2string(newExtendedState));

    // Also ignore this event if the request is over

    if (state() == State::FINISHED) return;

    // *IMPORTANT*: Set new state *BEFORE* calling subclass-specific cancellation
    // protocol to make sure all event handlers will recognize this scenario and
    // avoid making any modifications to the request's state.

    setState(lock,
             State::FINISHED,
             newExtendedState);

    // Invoke a subclass specific cancellation sequence of actions if anything
    // bad has happen.

    if (newExtendedState != ExtendedState::SUCCESS) cancelImpl(lock);

    controller()->serviceProvider()->databaseServices()->saveState(*this, options(lock));

    // Stop timers if they're still running

    if(_heartbeatTimerPtr)  _heartbeatTimerPtr->cancel();
    if(_expirationTimerPtr) _expirationTimerPtr->cancel();

    notify();
}

void Job::qservAddReplica(util::Lock const& lock,
                          unsigned int chunk,
                          std::vector<std::string> const& databases,
                          std::string const& worker,
                          AddReplicaQservMgtRequest::CallbackType onFinish) {

    LOGS(_log, LOG_LVL_DEBUG, context()
         << "** START ** Qserv notification on ADD replica:"
         << ", chunk="     << chunk
         << ", databases=" << util::printable(databases)
         << "  worker="    << worker);

    auto self = shared_from_this();

    controller()->serviceProvider()->qservMgtServices()->addReplica(
        chunk,
        databases,
        worker,
        [self,onFinish] (AddReplicaQservMgtRequest::Ptr const& request) {

            LOGS(_log, LOG_LVL_DEBUG, self->context()
                 << "** FINISH ** Qserv notification on ADD replica:"
                 << "  chunk="     << request->chunk()
                 << ", databases=" << util::printable(request->databases())
                 << ", worker="    << request->worker()
                 << ", state="     << request->state2string());

            // Pass the result to a caller

            if (onFinish) {
                onFinish(request);
            }
        },
        id()
    );
}

void Job::qservRemoveReplica(util::Lock const& lock,
                             unsigned int chunk,
                             std::vector<std::string> const& databases,
                             std::string const& worker,
                             bool force,
                             RemoveReplicaQservMgtRequest::CallbackType onFinish) {

    LOGS(_log, LOG_LVL_DEBUG, context()
         << "** START ** Qserv notification on REMOVE replica:"
         << "  chunk="     << chunk
         << ", databases=" << util::printable(databases)
         << ", worker="    << worker
         << ", force="     << (force ? "true" : "false"));

    auto self = shared_from_this();

    controller()->serviceProvider()->qservMgtServices()->removeReplica(
        chunk,
        databases,
        worker,
        force,
        [self,onFinish] (RemoveReplicaQservMgtRequest::Ptr const& request) {

            LOGS(_log, LOG_LVL_DEBUG, self->context()
                 << "** FINISH ** Qserv notification on REMOVE replica:"
                 << "  chunk="     << request->chunk()
                 << ", databases=" << util::printable(request->databases())
                 << ", worker="    << request->worker()
                 << ", force="     << (request->force() ? "true" : "false")
                 << ", state="     << request->state2string());

            // Pass the result to the caller
            if (onFinish) {
                onFinish(request);
            }
        },
        id()
    );
}

void Job::assertState(util::Lock const& lock,
                      State desiredState,
                      std::string const& context) const {
    if (desiredState != state()) {
        throw std::logic_error(
            context + ": wrong state " + state2string(state()) + " instead of " + state2string(desiredState));
    }
}

void Job::setState(util::Lock const& lock,
                   State newState,
                   ExtendedState newExtendedState) {

    LOGS(_log, LOG_LVL_DEBUG, context() << "setState  new state=" << state2string(newState, newExtendedState));

    // ATTENTION: changing the top-level state to FINISHED should be last step
    // in the transient state transition in order to ensure a consistent view
    // onto the combined state.

    if (state() == State::FINISHED) {
        _endTime = PerformanceUtils::now();
    }
    _extendedState = newExtendedState;
    _state = newState;

    controller()->serviceProvider()->databaseServices()->saveState(*this, options(lock));
}

void Job::startHeartbeatTimer(util::Lock const& lock) {

    if (_heartbeatTimerIvalSec) {

        LOGS(_log, LOG_LVL_DEBUG, context() << "startHeartbeatTimer");

        // The time needs to be initialized each time when a new interval
        // is about to begin. Otherwise it will strt firing immediately.
        _heartbeatTimerPtr.reset(
            new boost::asio::deadline_timer(
                controller()->io_service(),
                boost::posix_time::seconds(_heartbeatTimerIvalSec)));

        _heartbeatTimerPtr->async_wait(
            boost::bind(
                &Job::heartbeat,
                shared_from_this(),
                boost::asio::placeholders::error
            )
        );
    }
}

void Job::heartbeat(boost::system::error_code const& ec) {

    LOGS(_log, LOG_LVL_DEBUG, context() << "heartbeat: "
         << (ec == boost::asio::error::operation_aborted ? "** ABORTED **" : ""));

    // Ignore this event if the timer was aborted
    if (ec == boost::asio::error::operation_aborted) return;

    // IMPORTANT: the final state is required to be tested twice. The first time
    // it's done in order to avoid deadlock on the "in-flight" requests reporting
    // their completion while the job termination is in a progress. And the second
    // test is made after acquering the lock to recheck the state in case if it
    // has transitioned while acquering the lock.

    if (state() == State::FINISHED) return;

    util::Lock lock(_mtx, context() + "heartbeat");

    if (state() == State::FINISHED) return;

    // Update the job entry in the database

    controller()->serviceProvider()->databaseServices()->updateHeartbeatTime(*this);

    // Start another interval

    startHeartbeatTimer(lock);
}

void Job::startExpirationTimer(util::Lock const& lock) {

    if (0 != _expirationIvalSec) {

        LOGS(_log, LOG_LVL_DEBUG, context() << "startExpirationTimer");

        // The time needs to be initialized each time when a new interval
        // is about to begin. Otherwise it will strt firing immediately.
        _expirationTimerPtr.reset(
            new boost::asio::deadline_timer(
                controller()->io_service(),
                boost::posix_time::seconds(_expirationIvalSec)));

        _expirationTimerPtr->async_wait(
            boost::bind(
                &Job::expired,
                shared_from_this(),
                boost::asio::placeholders::error
            )
        );
    }
}

void Job::expired(boost::system::error_code const& ec) {

    LOGS(_log, LOG_LVL_DEBUG, context() << "expired: "
         << (ec == boost::asio::error::operation_aborted ? "** ABORTED **" : ""));

    // Ignore this event if the timer was aborted
    if (ec == boost::asio::error::operation_aborted) return;
         
    // IMPORTANT: the final state is required to be tested twice. The first time
    // it's done in order to avoid deadlock on the "in-flight" requests reporting
    // their completion while the job termination is in a progress. And the second
    // test is made after acquering the lock to recheck the state in case if it
    // has transitioned while acquering the lock.

    if (state() == State::FINISHED) return;

    util::Lock lock(_mtx, context() + "expired");

    if (state() == State::FINISHED) return;

    finish(lock, ExtendedState::TIMEOUT_EXPIRED);
}

void Job::notify() {

    // The callback is being made asynchronously in a separate thread
    // to avoid blocking the current thread.
    //
    // TODO: consider reimplementing this method to send notificatons
    //       via a thread pool & a queue.

    auto const self = shared_from_this();

    std::thread notifier([self]() {
        self->notifyImpl();
    });
    notifier.detach();
}

bool JobCompare::operator()(Job::Ptr const& lhs,
                            Job::Ptr const& rhs) const {
    LOGS(_log, LOG_LVL_DEBUG, "JobCompare::operator<(" << lhs->id() << "," << rhs->id() + ")");
    return lhs->options().priority < rhs->options().priority;
}

}}} // namespace lsst::qserv::replica
