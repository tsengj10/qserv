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
#ifndef LSST_QSERV_REPLICA_ADDREPLICAQSERVMGTREQUEST_H
#define LSST_QSERV_REPLICA_ADDREPLICAQSERVMGTREQUEST_H

/// AddReplicaQservMgtRequest.h declares:
///
/// class AddReplicaQservMgtRequest
/// (see individual class documentation for more information)

// System headers
#include <memory>
#include <string>
#include <vector>

// Qserv headers
#include "replica/QservMgtRequest.h"
#include "replica/ServiceProvider.h"
#include "wpublish/ChunkGroupQservRequest.h"

// This header declarations

namespace lsst {
namespace qserv {
namespace replica {

/**
  * Class AddReplicaQservMgtRequest implements a request notifying Qserv workers
  * on new chunks added to the database.
  */
class AddReplicaQservMgtRequest
    :   public QservMgtRequest  {

public:

    /// The pointer type for instances of the class
    typedef std::shared_ptr<AddReplicaQservMgtRequest> Ptr;

    /// The function type for notifications on the completon of the request
    typedef std::function<void(Ptr)> CallbackType;

    // Default construction and copy semantics are prohibited

    AddReplicaQservMgtRequest() = delete;
    AddReplicaQservMgtRequest(AddReplicaQservMgtRequest const&) = delete;
    AddReplicaQservMgtRequest& operator=(AddReplicaQservMgtRequest const&) = delete;

    ~AddReplicaQservMgtRequest() final = default;

    /**
     * Static factory method is needed to prevent issues with the lifespan
     * and memory management of instances created otherwise (as values or via
     * low-level pointers).
     *
     * @param serviceProvider - reference to a provider of services
     * @param io_service      - BOOST ASIO service
     * @param worker          - the name of a worker
     * @param chunk           - the chunk number
     * @param databases       - the names of databases
     * @param onFinish        - (optional) callback function to be called upon request completion
     *
     * @return pointer to the new object created by the factory
     */
    static Ptr create(ServiceProvider::Ptr const& serviceProvider,
                      boost::asio::io_service& io_service,
                      std::string const& worker,
                      unsigned int chunk,
                      std::vector<std::string> const& databases,
                      CallbackType onFinish = nullptr);

    /// @return the chunk number
    unsigned int chunk() const { return _chunk; }

    /// @return names of databases
    std::vector<std::string> const& databases() const { return _databases; }

    /**
     * Implement the corresponding method of the base class.
     *
     * @see QservMgtRequest::extendedPersistentState()
     */
     std::string extendedPersistentState(SqlGeneratorPtr const& gen) const override;

private:

    /**
     * Construct the request with the pointer to the services provider.
     *
     * @param serviceProvider - reference to a provider of services
     * @param io_service      - BOOST ASIO service
     * @param worker          - the name of a worker
     * @param chunk           - the chunk number
     * @param databases       - the names of databases
     * @param onFinish        - callback function to be called upon request completion
     */
    AddReplicaQservMgtRequest(ServiceProvider::Ptr const& serviceProvider,
                              boost::asio::io_service& io_service,
                              std::string const& worker,
                              unsigned int chunk,
                              std::vector<std::string> const& databases,
                              CallbackType onFinish);

    /**
      * Implememnt the corresponding method of the base class
      *
      * @see QservMgtRequest::startImpl
      */
    void startImpl(util::Lock const& lock) final;

    /**
      * Implememnt the corresponding method of the base class
      *
      * @see QservMgtRequest::finishImpl
      */
    void finishImpl(util::Lock const& lock) final;

    /**
      * Implememnt the corresponding method of the base class
      *
      * @see QservMgtRequest::notifyImpl
      */
    void notifyImpl() final;

private:

    /// The chunk number
    unsigned int _chunk;

    /// The names of databases
    std::vector<std::string> _databases;

    /// The callback function for sending a notification upon request completion
    CallbackType _onFinish;

    /// A request to the remote services
    wpublish::AddChunkGroupQservRequest::Ptr _qservRequest;
};

}}} // namespace lsst::qserv::replica

#endif // LSST_QSERV_REPLICA_ADDREPLICAQSERVMGTREQUEST_H
