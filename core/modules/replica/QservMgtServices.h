/*
 * LSST Data Management System
 * Copyright 2018 LSST Corporation.
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
#ifndef LSST_QSERV_REPLICA_QSERVMGTSERVICES_H
#define LSST_QSERV_REPLICA_QSERVMGTSERVICES_H

/// QservMgtServices.h declares:
///
/// class QservMgtServices
/// (see individual class documentation for more information)

// System headers
#include <map>
#include <memory>
#include <vector>

// Qserv headers
#include "replica/AddReplicaQservMgtRequest.h"
#include "replica/GetReplicasQservMgtRequest.h"
#include "replica/RemoveReplicaQservMgtRequest.h"
#include "replica/ServiceProvider.h"
#include "replica/SetReplicasQservMgtRequest.h"
#include "util/Mutex.h"

// Forward declarations
class XrdSsiService;

// This header declarations

namespace lsst {
namespace qserv {
namespace replica {

/**
 * Struct QservMgtRequestWrapper is an abstract base for implementing requests
 * registry as a polymorphic collection to store active requests. Pure virtual
 * methods of the class will be overriden by request-type-specific implementations
 * (see struct RequestWrappeImplr<REQUEST_TYPE> in the .cc file) capturing
 * type-dependant pointer and a callback function.
 */
struct QservMgtRequestWrapper {

    /// The pointer type for instances of the class
    typedef std::shared_ptr<QservMgtRequestWrapper> Ptr;

    virtual ~QservMgtRequestWrapper() = default;

    /**
     * This method (to be inplemented by subclasses) will be called upon
     * a completion of a request to notify a subscriber on the event.
     */
    virtual void notify() const=0;

    /// @return a pointer to the stored request object
    virtual std::shared_ptr<QservMgtRequest> request() const=0;
};

/**
  * Class QservMgtServices is a high-level interface to the Qserv management
  * services used by the replication system.
  */
class QservMgtServices
    :   public std::enable_shared_from_this<QservMgtServices> {

public:

    /// The pointer type for instances of the class
    typedef std::shared_ptr<QservMgtServices> Ptr;

    /**
     * The factory method for instamtiating a proper service object based
     * on an application configuration.
     *
     * @param configuration - the configuration service
     *
     * @return pointer to the created object
     */
    static Ptr create(ServiceProvider::Ptr const& serviceProvider);

    // Default construction and copy semantics are prohibited

    QservMgtServices() = delete;
    QservMgtServices(QservMgtServices const&) = delete;
    QservMgtServices& operator=(QservMgtServices const&) = delete;

    ~QservMgtServices() = default;

    /// @return reference to the ServiceProvider object
    ServiceProvider::Ptr const& serviceProvider() { return _serviceProvider; }

    /**
     * Notify Qserv worker on availability of a new replica
     *
     * @param chunk     - the chunk number
     * @param databases - the names of databases
     * @param worker    - the name of a worker where the replica is residing
     * @param onFinish  - callback function called on a completion of the operation
     * @param jobId     - an optional identifier of a job specifying a context
     *                    in which a request will be executed.
     * @param requestExpirationIvalSec - an optional parameter (if differs from 0)
     *                    allowing to override the default value of
     *                    the corresponding parameter from the Configuration.
     *
     * @return pointer to the request object if the request was made. Return
     *         nullptr otherwise.
     */
    AddReplicaQservMgtRequest::Ptr addReplica(
                                            unsigned int chunk,
                                            std::vector<std::string> const& databases,
                                            std::string const& worker,
                                            AddReplicaQservMgtRequest::CallbackType onFinish = nullptr,
                                            std::string const& jobId="",
                                            unsigned int requestExpirationIvalSec=0);

    /**
     * Notify Qserv worker on a removal of a replica
     *
     * @param chunk     - the chunk number
     * @param databases - the names of databases
     * @param worker    - the name of a worker where the replica is residing
     * @param force     - tell Qserv that the replica has to be removed from its
     *                    repository regardless if there are any outstanding requests
     *                    using the replica.
     * @param onFinish  - callback function called on a completion of the operation
     * @param jobId     - an optional identifier of a job specifying a context
     *                    in which a request will be executed.
     * @param requestExpirationIvalSec - an optional parameter (if differs from 0)
     *                    allowing to override the default value of
     *                    the corresponding parameter from the Configuration.
     *
     * @return pointer to the request object if the request was made. Return
     *         nullptr otherwise.
     */
    RemoveReplicaQservMgtRequest::Ptr removeReplica(
                                            unsigned int chunk,
                                            std::vector<std::string> const& databases,
                                            std::string const& worker,
                                            bool force,
                                            RemoveReplicaQservMgtRequest::CallbackType onFinish = nullptr,
                                            std::string const& jobId="",
                                            unsigned int requestExpirationIvalSec=0);
    /**
     * Fetch replicas known to a Qserv worker
     *
     * @param databaseFamily  - the name of a database family
     * @param worker          - the name of a worker
     * @param inUseOnly       - return replicas which're presently in use
     * @param onFinish        - callback function to be called upon request completion
     * @param jobId           - an optional identifier of a job specifying a context
     *                          in which a request will be executed.
     * @param requestExpirationIvalSec - an optional parameter (if differs from 0)
     *                          allowing to override the default value of
     *                          the corresponding parameter from the Configuration.
     *
     * @return pointer to the request object if the request was made. Return
     *         nullptr otherwise.
     */
    GetReplicasQservMgtRequest::Ptr getReplicas(
                                            std::string const& databaseFamily,
                                            std::string const& worker,
                                            bool inUseOnly = false,
                                            std::string const& jobId="",
                                            GetReplicasQservMgtRequest::CallbackType onFinish = nullptr,
                                            unsigned int requestExpirationIvalSec=0);



    /**
     * Set new replicas at the new replicas at a Qserv worker
     *
     * @param worker          - the name of a worker
     * @param newReplicas     - collection of new replicas (NOTE: useCount field is ignored)
     * @param force           - proceed with the operation even if some replicas affceted by
     *                          the operation are in use.
     * @param onFinish        - callback function to be called upon request completion
     * @param jobId           - an optional identifier of a job specifying a context
     *                          in which a request will be executed.
     * @param requestExpirationIvalSec - an optional parameter (if differs from 0)
     *                          allowing to override the default value of
     *                          the corresponding parameter from the Configuration.
     *
     * @return pointer to the request object if the request was made. Return
     *         nullptr otherwise.
     */
    SetReplicasQservMgtRequest::Ptr setReplicas(
                                            std::string const& worker,
                                            QservReplicaCollection const& newReplicas,
                                            bool force = false,
                                            std::string const& jobId="",
                                            SetReplicasQservMgtRequest::CallbackType onFinish = nullptr,
                                            unsigned int requestExpirationIvalSec=0);
private:

    /**
     * Construct the object.
     *
     * @param configuration - the configuration service
     */
    explicit QservMgtServices(ServiceProvider::Ptr const& serviceProvider);

    /**
     * Finalize the completion of the request. This method will notify
     * a requestor on the completion of the operation and it will also
     * remove the request from the server's registry.
     *
     * @param id - a unique identifier of a completed request
     */
    void finish(std::string const& id);

    /**
     * @return XROOTD/SSI API service for launching worker management requests.
     * The method is allowed to return the null pointer in case if a connection
     * to the service provider could not be established.
     */
    XrdSsiService* xrdSsiService();

private:

    ServiceProvider::Ptr _serviceProvider;

    // BOOST ASIO communication services

    boost::asio::io_service _io_service;
    std::unique_ptr<boost::asio::io_service::work> _work;

    /// The registry of the on-going requests.
    std::map<std::string,std::shared_ptr<QservMgtRequestWrapper>> _registry;

    /// The mutex for enforcing thread safety of the class's public API
    /// and internal operations.
    mutable util::Mutex _mtx;
};

}}} // namespace lsst::qserv::replica

#endif // LSST_QSERV_REPLICA_QSERVMGTSERVICES_H
