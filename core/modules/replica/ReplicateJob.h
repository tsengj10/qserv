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
#ifndef LSST_QSERV_REPLICA_REPLICATEJOB_H
#define LSST_QSERV_REPLICA_REPLICATEJOB_H

/// ReplicateJob.h declares:
///
/// struct ReplicateJobResult
/// class  ReplicateJob
///
/// (see individual class documentation for more information)

// System headers
#include <atomic>
#include <functional>
#include <list>
#include <map>
#include <string>

// Qserv headers
#include "replica/CreateReplicaJob.h"
#include "replica/Job.h"
#include "replica/FindAllJob.h"
#include "replica/ReplicaInfo.h"

// This header declarations

namespace lsst {
namespace qserv {
namespace replica {

/**
 * The structure ReplicateJobResult represents a combined result received
 * from worker services upon a completion of the job.
 */
struct ReplicateJobResult {

    /// Results reported by workers upon the successfull completion
    /// of the corresponidng replica creation jobs
    std::list<ReplicaInfo> replicas;

    /// Results groupped by: chunk number, database, worker
    std::map<unsigned int,                  // chunk
             std::map<std::string,          // database
                      std::map<std::string, // worker
                               ReplicaInfo>>> chunks;

    /// Per-worker flags indicating if the corresponidng replica creation
    /// job succeeded.
    std::map<std::string, bool> workers;
};

/**
  * Class ReplicateJob represents a tool which will increase the minimum
  * number of each chunk's replica up to the requested level.
  */
class ReplicateJob
    :   public Job  {

public:

    /// The pointer type for instances of the class
    typedef std::shared_ptr<ReplicateJob> Ptr;

    /// The function type for notifications on the completon of the job
    typedef std::function<void(Ptr)> CallbackType;

    /// @return default options object for this type of a job
    static Job::Options const& defaultOptions();

    /**
     * Static factory method is needed to prevent issue with the lifespan
     * and memory management of instances created otherwise (as values or via
     * low-level pointers).
     *
     * @param databaseFamily - name of a database family
     * @param numReplicas    - optional (if not 0) override for the minimum number of replicas
     *                         for each chunk. If the parameter is set to 0 then the corresponding
     *                         configuration option for the database family will be assumed.
     * @param controller     - for launching jobs
     * @param parentJobId    - optional identifier of a parent job
     * @param onFinish       - callback function to be called upon a completion of the job
     * @param options        - job options
     *
     * @return pointer to the created object
     */
    static Ptr create(std::string const& databaseFamily,
                      unsigned int numReplicas,
                      Controller::Ptr const& controller,
                      std::string const& parentJobId,
                      CallbackType onFinish,
                      Job::Options const& options=defaultOptions());

    // Default construction and copy semantics are prohibited

    ReplicateJob() = delete;
    ReplicateJob(ReplicateJob const&) = delete;
    ReplicateJob& operator=(ReplicateJob const&) = delete;

    /// Destructor (non-trivial in order to release chunks locked by the operation)
    ~ReplicateJob() final;

    /**
     * @return the minimum number of each chunk's replicas to be reached when
     * the job successfully finishes.
     */
    unsigned int numReplicas() const { return _numReplicas; }

    /// @return the name of a database family defining a scope of the operation
    std::string const& databaseFamily() const { return _databaseFamily; }

    /**
     * Return the result of the operation.
     *
     * IMPORTANT NOTES:
     * - the method should be invoked only after the job has finished (primary
     *   status is set to Job::Status::FINISHED). Otherwise exception
     *   std::logic_error will be thrown
     *
     * - the result will be extracted from the replica creation which have successfully
     *   finished. Please, verify the primary and extended status of the object
     *   to ensure that all jobs have finished.
     *
     * @return the data structure to be filled upon the completin of the job.
     *
     * @throws std::logic_error - if the job dodn't finished at a time
     *                            when the method was called
     */
    ReplicateJobResult const& getReplicaData() const;

    /**
     * @see Job::extendedPersistentState()
     */
    std::string extendedPersistentState(SqlGeneratorPtr const& gen) const override;

protected:

    /**
     * Construct the job with the pointer to the services provider.
     *
     * @see ReplicateJob::create()
     */
    ReplicateJob(std::string const& databaseFamily,
                 unsigned int numReplicas,
                 Controller::Ptr const& controller,
                 std::string const& parentJobId,
                 CallbackType onFinish,
                 Job::Options const& options);

    /**
      * @see Job::startImpl()
      */
    void startImpl(util::Lock const& lock) final;

    /**
      * @see Job::startImpl()
      */
    void cancelImpl(util::Lock const& lock) final;

    /**
      * @see Job::notifyImpl()
      */
    void notifyImpl() final;

    /**
     * The calback function to be invoked on a completion of the precursor job
     * which harvests chunk disposition accross relevant worker nodes.
     */
    void onPrecursorJobFinish();

    /**
     * The calback function to be invoked on a completion of each replication job
     *
     * @param job - pointer to a job
     */
    void onCreateJobFinish(CreateReplicaJob::Ptr const& job);

    /**
     * Restart the job from scratch. This method will reset object context
     * to a state it was before method Job::startImpl() called and then call
     * Job::startImpl() again.
     *
     * @param lock - the lock must be acquired by a caller of the method
     */
    void restart(util::Lock const& lock);

    /**
     * Unconditionally release the specified chunk
     *
     * @param chunk - the chunk number
     */
    void release(unsigned int chunk);

protected:

    /// The name of the database family
    std::string _databaseFamily;

    /// The minimum number of replicas for each chunk
    unsigned int _numReplicas;

    /// Client-defined function to be called upon the completion of the job
    CallbackType _onFinish;

    /// The chained job to be completed first in order to figure out
    /// replica disposition.
    FindAllJob::Ptr _findAllJob;

    /// The total number of iterations the job has gone so far
    size_t _numIterations;

    /// The number of chunks which require the replication but couldn't be locked
    /// in the exclusive mode. The counter will be analyzed upon a completion
    /// of the last replica creation job, and if it were found not empty another
    /// iteration of the job will be undertaken
    size_t _numFailedLocks;

    /// A collection of jobs groupped by the corresponidng chunk
    /// number. The main idea is simplify tracking the completion status
    /// of the operation on each chunk. Requests will be added to the
    /// corresponding group as they're launched, and removed when they
    /// finished. This allows releasing (unlocking) chunks before
    /// the whole job finishes.
    ///
    /// [chunk][worker]
    //
    std::map<unsigned int,
             std::map<std::string,
                      CreateReplicaJob::Ptr>> _chunk2jobs;

    /// A collection of replica creation jobs implementing the operation
    std::list<CreateReplicaJob::Ptr> _jobs;

    // The counter of jobs which will be updated. They need to be atomic
    // to avoid race condition between the onFinish() callbacks executed within
    // the Controller's thread and this thread.

    std::atomic<size_t> _numLaunched;   ///< the total number of replica creation jobs launched
    std::atomic<size_t> _numFinished;   ///< the total number of finished jobs
    std::atomic<size_t> _numSuccess;    ///< the number of successfully completed jobs

    /// The result of the operation (gets updated as jobs are finishing)
    ReplicateJobResult _replicaData;
};

}}} // namespace lsst::qserv::replica

#endif // LSST_QSERV_REPLICA_REPLICATEJOB_H
