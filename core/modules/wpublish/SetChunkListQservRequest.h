// -*- LSST-C++ -*-
/*
 * LSST Data Management System
 * Copyright 2011-2018 LSST Corporation.
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
/// SetChunkListQservRequest.h
#ifndef LSST_QSERV_WPUBLISH_SET_CHUNK_LIST_QSERV_REQUEST_H
#define LSST_QSERV_WPUBLISH_SET_CHUNK_LIST_QSERV_REQUEST_H

// System headers
#include <functional>
#include <list>
#include <memory>

// Third party headers

// Qserv headers
#include "wpublish/QservRequest.h"

// Forward declarations

namespace lsst {
namespace qserv {
namespace wpublish {

/**
  * Class SetChunkListQservRequest inplements the client-side requests
  * the Qserv worker services for a status of chunk lists.
  */
class SetChunkListQservRequest
    :    public QservRequest {

public:

    /// Completion status of the operation
    enum Status {
        SUCCESS,    // successful completion of a request
        INVALID,    // invalid parameters of the request
        IN_USE,     // request is rejected because one of the chunks is in use
        ERROR       // an error occured during command execution
    };

    /// @return string representation of a status
    static std::string status2str(Status status);

    /// Struct Chunk a value type encapsulating a chunk number and the name
    /// of a database
    struct Chunk {
        unsigned int chunk;
        std::string  database;
        unsigned int use_count;
    };

    /// The ChunkCollection type refresens a collection of chunks
    using ChunkCollection = std::list<Chunk>;

    /// The pointer type for instances of the class
    typedef std::shared_ptr<SetChunkListQservRequest> Ptr;

    /// The callback function type to be used for notifications on
    /// the operation completion.
    using CallbackType =
        std::function<void(Status,                      // completion status
                           std::string const&,          // error message
                           ChunkCollection const&)>;    // chunks (if success)
    /**
     * Static factory method is needed to prevent issues with the lifespan
     * and memory management of instances created otherwise (as values or via
     * low-level pointers).
     *
     * ATTENTION: the 'use_count' field of structure Chunk is ignored by this
     * class when used on its input.
     *
     * @param chunks    - collection of chunks to be transferred to the worker
     * @param force     - force the proposed change even if the chunk is in use
     * @param onFinish  - optional callback function to be called upon the completion
     *                    (successful or not) of the request.
     * @return smart pointer to the object of the class
     */
   static Ptr create(ChunkCollection const& chunks,
                     bool force = false,
                     CallbackType onFinish = nullptr);

    // Default onstruction and copy semantics are prohibited
    SetChunkListQservRequest() = delete;
    SetChunkListQservRequest(SetChunkListQservRequest const&) = delete;
    SetChunkListQservRequest& operator=(SetChunkListQservRequest const&) = delete;

    /// Destructor
    ~SetChunkListQservRequest() override;

protected:

    /**
     * Normal constructor
     *
     * ATTENTION: the 'use_count' field of structure Chunk is ignored by this
     * class when used on its input.
     *
     * @param chunks    - collection of chunks to be transferred to the worker
     * @param force     - force the proposed change even if the chunk is in use
     * @param onFinish  - optional callback function to be called upon the completion
     *                    (successful or not) of the request.
     */
    SetChunkListQservRequest(ChunkCollection const& chunks,
                             bool force,
                             CallbackType onFinish);

    /// Implement the corresponding method of the base class
    void onRequest(proto::FrameBuffer& buf) override;

    /// Implement the corresponding method of the base class
    void onResponse(proto::FrameBufferView& view) override;

    /// Implement the corresponding method of the base class
    void onError(std::string const& error) override;

private:

    ChunkCollection _chunks;
    bool _force;

    /// Optional callback function to be called upon the completion
    /// (successfull or not) of the request.
    CallbackType _onFinish;
};

}}} // namespace lsst::qserv::wpublish

#endif // LSST_QSERV_WPUBLISH_SET_CHUNK_LIST_QSERV_REQUEST_H
