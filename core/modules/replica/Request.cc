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
#include "replica/Request.h"

// System headers
#include <stdexcept>
#include <thread>

// Third party headers
#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

// Qserv headers
#include "lsst/log/Log.h"
#include "replica/Configuration.h"
#include "replica/Controller.h"
#include "replica/ProtocolBuffer.h"
#include "replica/ServiceProvider.h"

namespace {

LOG_LOGGER _log = LOG_GET("lsst.qserv.replica.Request");

} /// namespace

namespace lsst {
namespace qserv {
namespace replica {

std::string Request::state2string(State state) {
    switch (state) {
        case CREATED:     return "CREATED";
        case IN_PROGRESS: return "IN_PROGRESS";
        case FINISHED:    return "FINISHED";
    }
    throw std::logic_error(
                    "incomplete implementation of method Request::state2string(State)");
}

std::string Request::state2string(ExtendedState state) {
    switch (state) {
        case NONE:                 return "NONE";
        case SUCCESS:              return "SUCCESS";
        case CLIENT_ERROR:         return "CLIENT_ERROR";
        case SERVER_BAD:           return "SERVER_BAD";
        case SERVER_ERROR:         return "SERVER_ERROR";
        case SERVER_QUEUED:        return "SERVER_QUEUED";
        case SERVER_IN_PROGRESS:   return "SERVER_IN_PROGRESS";
        case SERVER_IS_CANCELLING: return "SERVER_IS_CANCELLING";
        case SERVER_CANCELLED:     return "SERVER_CANCELLED";
        case TIMEOUT_EXPIRED:      return "TIMEOUT_EXPIRED";
        case CANCELLED:            return "CANCELLED";
    }
    throw std::logic_error(
                    "incomplete implementation of method Request::state2string(ExtendedState)");
}

std::string Request::state2string(State state,
                                  ExtendedState extendedState) {
    return state2string(state) + "::" + state2string(extendedState);
}

std::string Request::state2string(State state,
                                  ExtendedState extendedState,
                                  replica::ExtendedCompletionStatus serverStatus) {
    return state2string(state, extendedState) + "::" + replica::status2string(serverStatus);
}

Request::Request(ServiceProvider::Ptr const& serviceProvider,
                 boost::asio::io_service& io_service,
                 std::string const& type,
                 std::string const& worker,
                 int  priority,
                 bool keepTracking,
                 bool allowDuplicate)
    :   _serviceProvider(serviceProvider),
        _type(type),
        _id(Generators::uniqueId()),
        _worker(worker),
        _priority(priority),
        _keepTracking(keepTracking),
        _allowDuplicate(allowDuplicate),
        _state(CREATED),
        _extendedState(NONE),
        _extendedServerStatus(ExtendedCompletionStatus::EXT_STATUS_NONE),
        _bufferPtr(new ProtocolBuffer(serviceProvider->config()->requestBufferSizeBytes())),
        _workerInfo(serviceProvider->config()->workerInfo(worker)),
        _timerIvalSec(serviceProvider->config()->retryTimeoutSec()),
        _timer(io_service),
        _requestExpirationIvalSec(serviceProvider->config()->controllerRequestTimeoutSec()),
        _requestExpirationTimer(io_service) {

    _serviceProvider->assertWorkerIsValid(worker);
}

std::string Request::state2string() const {
    util::Lock lock(_mtx, context() + "state2string");
    return state2string(state(), extendedState()) + "::" + replica::status2string(extendedServerStatus());
}

std::string Request::context() const {
    return "REQUEST " + id() + "  " + type() +
           "  " + state2string(state(), extendedState()) +
           "::" + replica::status2string(extendedServerStatus()) + "  ";
}

std::string const& Request::remoteId() const {
    return _duplicateRequestId.empty() ? _id : _duplicateRequestId;
}

Performance Request::performance() const {
    util::Lock lock(_mtx, context() + "performance");
    return performance(lock);
}

Performance Request::performance(util::Lock const& lock) const {
    return _performance;
}

void Request::start(std::shared_ptr<Controller> const& controller,
                    std::string const& jobId,
                    unsigned int requestExpirationIvalSec) {

    util::Lock lock(_mtx, context() + "start");

    assertState(lock,
                CREATED,
                context() + "start");

    // Change the expiration ival if requested
    if (requestExpirationIvalSec) {
        _requestExpirationIvalSec = requestExpirationIvalSec;
    }
    LOGS(_log, LOG_LVL_DEBUG, context() << "start  _requestExpirationIvalSec: "
         << _requestExpirationIvalSec);

    // Build optional associaitons with the corresponding Controller and the job
    //
    // NOTE: this is done only once, the first time a non-trivial value
    // of each parameter is presented to the method.

    if (not _controller    and     controller)    _controller = controller;
    if (    _jobId.empty() and not jobId.empty()) _jobId      = jobId;

    _performance.setUpdateStart();

    if (_requestExpirationIvalSec) {
        _requestExpirationTimer.cancel();
        _requestExpirationTimer.expires_from_now(boost::posix_time::seconds(_requestExpirationIvalSec));
        _requestExpirationTimer.async_wait(
            boost::bind(
                &Request::expired,
                shared_from_this(),
                boost::asio::placeholders::error
            )
        );
    }

    // Let a subclass to proceed with its own sequence of actions

    startImpl(lock);

    // Finalize state transition before saving the persistent state

    setState(lock, IN_PROGRESS);
}

std::string const& Request::jobId() const {
    if (state() == State::CREATED) {
        throw std::logic_error(
            "the Job Id is not available because the request has not started yet");
    }
    return _jobId;
}

void Request::expired(boost::system::error_code const& ec) {

    LOGS(_log, LOG_LVL_DEBUG, context() << "expired"
         << (ec == boost::asio::error::operation_aborted ? "  ** ABORTED **" : ""));

    // Ignore this event if the timer was aborted

    if (ec == boost::asio::error::operation_aborted) return;

    // IMPORTANT: the final state is required to be tested twice. The first time
    // it's done in order to avoid deadlock on the "in-flight" callbacks reporting
    // their completion while the request termination is in a progress. And the second
    // test is made after acquering the lock to recheck the state in case if it
    // has transitioned while acquering the lock.

    if (state() == State::FINISHED) return;

    util::Lock lock(_mtx, context() + "expired");

    if (state() == State::FINISHED) return;

    finish(lock, TIMEOUT_EXPIRED);
}

void Request::cancel() {

    LOGS(_log, LOG_LVL_DEBUG, context() << "cancel");

    // IMPORTANT: the final state is required to be tested twice. The first time
    // it's done in order to avoid deadlock on the "in-flight" callbacks reporting
    // their completion while the request termination is in a progress. And the second
    // test is made after acquering the lock to recheck the state in case if it
    // has transitioned while acquering the lock.

    if (state() == State::FINISHED) return;

    util::Lock lock(_mtx, context() + "cancel");

    if (state() == FINISHED) return;

    finish(lock, CANCELLED);
}

void Request::finish(util::Lock const& lock,
                     ExtendedState extendedState) {

    LOGS(_log, LOG_LVL_DEBUG, context() << "finish");

    // Check if it's not too late for this operation

    if (state() == FINISHED) return;

    // We have to update the timestamp before making a state transition
    // to ensure a client gets a consistent view onto the object's state.

    _performance.setUpdateFinish();

    // Set new state to make sure all event handlers will recognize
    // this scenario and avoid making any modifications to the request's state.

    setState(lock,
             FINISHED,
             extendedState);

    // Stop the timer if the one is still running

    _requestExpirationTimer.cancel();

    // Let a subclass to run its own finalization if needed

    finishImpl(lock);

    notify();
}

void Request::notify() {

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

bool Request::isAborted(boost::system::error_code const& ec) const {

    if (ec == boost::asio::error::operation_aborted) {
        LOGS(_log, LOG_LVL_DEBUG, context() << "isAborted  ** ABORTED **");
        return true;
    }
    return false;
}

void Request::assertState(util::Lock const& lock,
                          State desiredState,
                          std::string const& context) const {

    if (desiredState != state()) {
        throw std::logic_error(
            context + ": wrong state " + state2string(state()) + " instead of " + state2string(desiredState));
    }
}

void Request::setState(util::Lock const& lock,
                       State newState,
                       ExtendedState newExtendedState)
{
    LOGS(_log, LOG_LVL_DEBUG, context() << "setState  " << state2string(newState, newExtendedState));

    // ATTENTION: ensure the top-level state is the last to change in
    // in the transient state transition in order to guarantee a consistent
    // view on to the object's state from the clients' prospective.

    _extendedState = newExtendedState;
    _state = newState;

    savePersistentState(lock);
}

}}} // namespace lsst::qserv::replica
