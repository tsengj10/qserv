// -*- LSST-C++ -*-
/*
 * LSST Data Management System
 * Copyright 2012-2018 AURA/LSST.
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
#include "wpublish/GetChunkListCommand.h"

// System headers

// Third-party headers

// LSST headers
#include "lsst/log/Log.h"
#include "proto/worker.pb.h"
#include "wbase/SendChannel.h"
#include "wpublish/ChunkInventory.h"
#include "wpublish/ResourceMonitor.h"

// Qserv headers

namespace {

LOG_LOGGER _log = LOG_GET("lsst.qserv.wpublish.GetChunkListCommand");

} // annonymous namespace

namespace lsst {
namespace qserv {
namespace wpublish {

GetChunkListCommand::GetChunkListCommand(std::shared_ptr<wbase::SendChannel> const& sendChannel,
                                         std::shared_ptr<ChunkInventory>     const& chunkInventory,
                                         std::shared_ptr<ResourceMonitor>    const& resourceMonitor)
    :   wbase::WorkerCommand(sendChannel),
        _chunkInventory(chunkInventory),
        _resourceMonitor(resourceMonitor) {
}

void GetChunkListCommand::reportError(std::string const& message) {

    LOGS(_log, LOG_LVL_ERROR, "GetChunkListCommand::run  " << message);

    proto::WorkerCommandGetChunkListR reply;

    reply.set_status(proto::WorkerCommandGetChunkListR::ERROR);
    reply.set_error(message);

    _frameBuf.serialize(reply);
    std::string str(_frameBuf.data(), _frameBuf.size());
    _sendChannel->sendStream(xrdsvc::StreamBuffer::createWithMove(str), true);
}

void GetChunkListCommand::run() {

    LOGS(_log, LOG_LVL_DEBUG, "GetChunkListCommand::run");

    proto::WorkerCommandGetChunkListR reply;
    reply.set_status(proto::WorkerCommandGetChunkListR::SUCCESS);

    ChunkInventory::ExistMap const existMap = _chunkInventory->existMap();

    for (auto const& entry: existMap) {
        std::string const& db = entry.first;

        for (int chunk: entry.second) {
            proto::WorkerCommandChunk* ptr = reply.add_chunks();
            ptr->set_db(db);
            ptr->set_chunk(chunk);
            ptr->set_use_count(_resourceMonitor->count(chunk, db));
        }
    }

    _frameBuf.serialize(reply);
    std::string str(_frameBuf.data(), _frameBuf.size());
    _sendChannel->sendStream(xrdsvc::StreamBuffer::createWithMove(str), true);
}

}}} // namespace lsst::qserv::wpublish
