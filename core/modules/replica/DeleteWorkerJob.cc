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
#include "replica/DeleteWorkerJob.h"

// System headers
#include <algorithm>
#include <atomic>
#include <future>
#include <stdexcept>
#include <tuple>

// Qserv headers
#include "lsst/log/Log.h"
#include "replica/Configuration.h"
#include "replica/DatabaseMySQL.h"
#include "replica/DatabaseServices.h"
#include "replica/ErrorReporting.h"
#include "replica/ServiceManagementRequest.h"
#include "replica/ServiceProvider.h"
#include "util/BlockPost.h"

namespace {

LOG_LOGGER _log = LOG_GET("lsst.qserv.replica.DeleteWorkerJob");

/**
 * Count the total number of entries in the input collection,
 * the number of finished entries, nd the total number of succeeded
 * entries.
 *
 * The "entries" in this context are either derivatives of the Request
 * or Job types.
 *
 * @param collection - a collection of entries to be analyzed
 * @return - a tuple of three elements
 */
template <class T>
std::tuple<size_t,size_t,size_t> counters(std::list<typename T::Ptr> const& collection) {
    size_t total    = 0;
    size_t finished = 0;
    size_t success  = 0;
    for (auto&& ptr: collection) {
        total++;
        if (ptr->state() == T::State::FINISHED) {
            finished++;
            if (ptr->extendedState() == T::ExtendedState::SUCCESS) {
                success++;
            }
        }
    }
    return std::make_tuple(total, finished, success);
}

} /// namespace

namespace lsst {
namespace qserv {
namespace replica {

Job::Options const& DeleteWorkerJob::defaultOptions() {
    static Job::Options const options{
        2,      /* priority */
        true,   /* exclusive */
        false   /* exclusive */
    };
    return options;
}

DeleteWorkerJob::Ptr DeleteWorkerJob::create(std::string const& worker,
                                                 bool permanentDelete,
                                                 Controller::Ptr const& controller,
                                                 std::string const& parentJobId,
                                                 CallbackType onFinish,
                                                 Job::Options const& options) {
    return DeleteWorkerJob::Ptr(
        new DeleteWorkerJob(worker,
                            permanentDelete,
                            controller,
                            parentJobId,
                            onFinish,
                            options));
}

DeleteWorkerJob::DeleteWorkerJob(std::string const& worker,
                                 bool permanentDelete,
                                 Controller::Ptr const& controller,
                                 std::string const& parentJobId,
                                 CallbackType onFinish,
                                 Job::Options const& options)
    :   Job(controller,
            parentJobId,
            "DELETE_WORKER",
            options),
        _worker(worker),
        _permanentDelete(permanentDelete),
        _onFinish(onFinish),
        _numLaunched(0),
        _numFinished(0),
        _numSuccess(0) {
}

DeleteWorkerJobResult const& DeleteWorkerJob::getReplicaData() const {

    LOGS(_log, LOG_LVL_DEBUG, context());

    if (state() == State::FINISHED) return _replicaData;

    throw std::logic_error(
        "DeleteWorkerJob::getReplicaData()  the method can't be called while the job hasn't finished");
}

std::string DeleteWorkerJob::extendedPersistentState(SqlGeneratorPtr const& gen) const {
    return gen->sqlPackValues(id(),
                              worker(),
                              permanentDelete() ? 1 : 0);
}

void DeleteWorkerJob::startImpl(util::Lock const& lock) {

    LOGS(_log, LOG_LVL_DEBUG, context() << "startImpl");

    util::BlockPost blockPost(1000, 2000);

    auto self = shared_from_base<DeleteWorkerJob>();

    // Check the status of the worker service, and if it's still running
    // try to get as much info from it as possible

    std::atomic<bool> statusRequestFinished{false};

    auto const statusRequest = controller()->statusOfWorkerService(
        worker(),
        [&statusRequestFinished](ServiceStatusRequest::Ptr const& request) {
            statusRequestFinished = true;
        },
        id(),   /* jobId */
        60      /* requestExpirationIvalSec */
    );
    while (not statusRequestFinished) {
        LOGS(_log, LOG_LVL_DEBUG, context() << "wait for worker service status");
        blockPost.wait();
    }
    if (statusRequest->extendedState() == Request::ExtendedState::SUCCESS) {
        if (statusRequest->getServiceState().state == ServiceState::State::RUNNING) {

            // Make sure the service won't be executing any other "leftover"
            // requests which may be interfeering with the current job's requests

            std::atomic<bool> drainRequestFinished{false};

            auto const drainRequest = controller()->drainWorkerService(
                worker(),
                [&drainRequestFinished](ServiceDrainRequest::Ptr const& request) {
                    drainRequestFinished = true;
                },
                id(),   /* jobId */
                60      /* requestExpirationIvalSec */
            );
            while (not drainRequestFinished) {
                LOGS(_log, LOG_LVL_DEBUG, context() << "wait for worker service drain");
                blockPost.wait();
            }
            if (drainRequest->extendedState() == Request::ExtendedState::SUCCESS) {
                if (drainRequest->getServiceState().state == ServiceState::State::RUNNING) {

                    // Try to get the most recent state the worker's replicas
                    // for all known databases

                    bool const saveReplicInfo = true;   // always save the replica info in a database because
                                                        // the algorithm depends on it.

                    for (auto&& database: controller()->serviceProvider()->config()->databases()) {
                        auto const request = controller()->findAllReplicas(
                            worker(),
                            database,
                            saveReplicInfo,
                            [self] (FindAllRequest::Ptr const& request) {
                                self->onRequestFinish(request);
                            }
                        );
                        _findAllRequests.push_back(request);
                        _numLaunched++;
                    }

                    // The rest will be happening in a method processing the completion
                    // of the above launched requests.

                    setState(lock, State::IN_PROGRESS);
                    return;
                }
            }
        }
    }

    // Since the worker is not available then go straight to a point
    // at which we'll be changing its state within the replication system

    disableWorker(lock);

    setState(lock, State::IN_PROGRESS);
    return;
}

void DeleteWorkerJob::cancelImpl(util::Lock const& lock) {

    LOGS(_log, LOG_LVL_DEBUG, context() << "cancelImpl");

    // To ensure no lingering "side effects" will be left after cancelling this
    // job the request cancellation should be also followed (where it makes a sense)
    // by stopping the request at corresponding worker service.

    for (auto&& ptr: _findAllRequests) {
        ptr->cancel();
        if (ptr->state() != Request::State::FINISHED) {
            controller()->stopReplicaFindAll(
                ptr->worker(),
                ptr->id(),
                nullptr,    /* onFinish */
                true,       /* keepTracking */
                id()         /* jobId */);
        }
    }

    // Stop chained jobs (if any) as well

    for (auto&& ptr: _findAllJobs)   ptr->cancel();
    for (auto&& ptr: _replicateJobs) ptr->cancel();
}

void DeleteWorkerJob::onRequestFinish(FindAllRequest::Ptr const& request) {

    LOGS(_log, LOG_LVL_DEBUG, context()
         << "onRequestFinish"
         << "  worker="   << request->worker()
         << "  database=" << request->database());

    // IMPORTANT: the final state is required to be tested twice. The first time
    // it's done in order to avoid deadlock on the "in-flight" requests reporting
    // their completion while the job termination is in a progress. And the second
    // test is made after acquering the lock to recheck the state in case if it
    // has transitioned while acquering the lock.
    
    if (state() == State::FINISHED) return;

    util::Lock lock(_mtx, context() + "onRequestFinish");

    if (state() == State::FINISHED) return;

    _numFinished++;
    if (request->extendedState() == Request::ExtendedState::SUCCESS) _numSuccess++;

    // Evaluate the status of on-going operations to see if the job
    // has finished. If so then proceed to the next stage of the job.
    //
    // ATTENTION: we don't care about the completion status of the requests
    // because the're related to a worker which is going to be removed, and
    // this worker may already be experiencing problems.
    //
    if (_numFinished == _numLaunched) disableWorker(lock);
}

void
DeleteWorkerJob::disableWorker(util::Lock const& lock) {

    LOGS(_log, LOG_LVL_DEBUG, context() << "disableWorker");

    // Temporary disable this worker from the configuration. If it's requsted
    // to be permanently deleted this will be done only after all other relevamnt
    // operations of this job will be done.

    controller()->serviceProvider()->config()->disableWorker(worker());

    // Launch the chained jobs to get chunk disposition within the rest
    // of the cluster

    _numLaunched = 0;
    _numFinished = 0;
    _numSuccess  = 0;

    auto self = shared_from_base<DeleteWorkerJob>();

    bool const saveReplicInfo = true;   // always save the replica info in a database because
                                        // the algorithm depends on it.

    for (auto&& databaseFamily: controller()->serviceProvider()->config()->databaseFamilies()) {
        FindAllJob::Ptr job = FindAllJob::create(
            databaseFamily,
            saveReplicInfo,
            controller(),
            id(),
            [self] (FindAllJob::Ptr job) {
                self->onJobFinish(job);
            }
        );
        job->start();
        _findAllJobs.push_back(job);
        _numLaunched++;
    }
}

void DeleteWorkerJob::onJobFinish(FindAllJob::Ptr const& job) {

    LOGS(_log, LOG_LVL_DEBUG, context() << "onJobFinish(FindAllJob) "
         << " databaseFamily: " << job->databaseFamily());

    // IMPORTANT: the final state is required to be tested twice. The first time
    // it's done in order to avoid deadlock on the "in-flight" requests reporting
    // their completion while the job termination is in a progress. And the second
    // test is made after acquering the lock to recheck the state in case if it
    // has transitioned while acquering the lock.
    
    if (state() == State::FINISHED) return;

    util::Lock lock(_mtx, context() + "onJobFinish(FindAllJob)");

    if (state() == State::FINISHED) return;

    _numFinished++;

    if (job->extendedState() != ExtendedState::SUCCESS) {
        finish(lock, ExtendedState::FAILED);
        return;
    }

    // Process the normal completion of the child job

    _numSuccess++;

    if (_numFinished == _numLaunched) {

        // Launch chained jobs to ensure the minimal replication level
        // which might be affected by the worker removal.

        _numLaunched = 0;
        _numFinished = 0;
        _numSuccess  = 0;

        auto self = shared_from_base<DeleteWorkerJob>();

        for (auto&& databaseFamily: controller()->serviceProvider()->config()->databaseFamilies()) {
            ReplicateJob::Ptr const job = ReplicateJob::create(
                databaseFamily,
                0,  /* numReplicas -- pull from Configuration */
                controller(),
                id(),
                [self] (ReplicateJob::Ptr job) {
                    self->onJobFinish(job);
                }
            );
            job->start();
            _replicateJobs.push_back(job);
            _numLaunched++;
        }
    }
}

void DeleteWorkerJob::onJobFinish(ReplicateJob::Ptr const& job) {

    LOGS(_log, LOG_LVL_DEBUG, context() << "onJobFinish(ReplicateJob) "
         << " databaseFamily: " << job->databaseFamily()
         << " numReplicas: " << job->numReplicas()
         << " state: " << job->state2string());

    // IMPORTANT: the final state is required to be tested twice. The first time
    // it's done in order to avoid deadlock on the "in-flight" requests reporting
    // their completion while the job termination is in a progress. And the second
    // test is made after acquering the lock to recheck the state in case if it
    // has transitioned while acquering the lock.
    
    if (state() == State::FINISHED) return;

    util::Lock lock(_mtx, context() + "onJobFinish(ReplicateJob)");

    if (state() == State::FINISHED) return;

    _numFinished++;

    if (job->extendedState() != ExtendedState::SUCCESS) {
        finish(lock, ExtendedState::FAILED);
        return;
    }

    // Process the normal completion of the child job

    _numSuccess++;

    LOGS(_log, LOG_LVL_DEBUG, context() << "onJobFinish(ReplicateJob)  "
         << "job->getReplicaData().chunks.size(): " << job->getReplicaData().chunks.size());

    // Merge results into the current job's result object
    _replicaData.chunks[job->databaseFamily()] = job->getReplicaData().chunks;

    if (_numFinished == _numLaunched) {

        // Construct a collection of orphan replicas if possible

        ReplicaInfoCollection replicas;
        if (controller()->serviceProvider()->databaseServices()->findWorkerReplicas(replicas, worker())) {
            for (ReplicaInfo const& replica: replicas) {
                unsigned int const chunk    = replica.chunk();
                std::string const& database = replica.database();

                bool replicated = false;
                for (auto&& databaseFamilyEntry: _replicaData.chunks) {
                    auto const& chunks = databaseFamilyEntry.second;
                    replicated = replicated or
                        (chunks.count(chunk) and chunks.at(chunk).count(database));
                }
                if (not replicated) {
                    _replicaData.orphanChunks[chunk][database] = replica;
                }
            }
        }

        // TODO: if the list of orphan chunks is not empty then consider bringing
        // back the disabled worker (if the service still responds) in the read-only
        // mode and try using it for redistributing those chunks accross the cluster.
        //
        // NOTE: this could be a complicated procedure which needs to be thought
        // through.
        ;

        // Do this only if requested, and only in case of the successful
        // completion of the job
        if (permanentDelete()) {
            controller()->serviceProvider()->config()->deleteWorker(worker());
        }
        finish(lock, ExtendedState::SUCCESS);
    }
}

void DeleteWorkerJob::notifyImpl() {

    LOGS(_log, LOG_LVL_DEBUG, context() << "notifyImpl");

    if (_onFinish) {
        _onFinish(shared_from_base<DeleteWorkerJob>());
    }
}

}}} // namespace lsst::qserv::replica
