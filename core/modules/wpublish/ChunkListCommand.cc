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
#include "wpublish/ChunkListCommand.h"

// System headers
#include <sstream>

// Third-party headers
#include "XrdSsi/XrdSsiCluster.hh"

// LSST headers
#include "lsst/log/Log.h"
#include "proto/worker.pb.h"
#include "wbase/SendChannel.h"
#include "wpublish/ChunkInventory.h"
#include "xrdsvc/SsiProvider.h"
#include "xrdsvc/XrdName.h"

/******************************************************************************/
/*                               G l o b a l s                                */
/******************************************************************************/

extern XrdSsiProvider* XrdSsiProviderLookup;

// Qserv headers

namespace {

LOG_LOGGER _log = LOG_GET("lsst.qserv.wpublish.ChunkListCommand");

/// Print the inventory status onto the logging stream
void dumpInventory(lsst::qserv::wpublish::ChunkInventory const& inventory,
                   std::string                           const& context) {
    std::ostringstream os;
    inventory.dbgPrint(os);
    LOGS(_log, LOG_LVL_DEBUG, context << os.str());
}

} // annonymous namespace

namespace lsst {
namespace qserv {
namespace wpublish {

ChunkListCommand::ChunkListCommand(std::shared_ptr<wbase::SendChannel> const& sendChannel,
                                   std::shared_ptr<ChunkInventory>     const& chunkInventory,
                                   mysql::MySqlConfig                  const& mySqlConfig,
                                   bool rebuild,
                                   bool reload)
    :   wbase::WorkerCommand(sendChannel),
        _chunkInventory(chunkInventory),
        _mySqlConfig   (mySqlConfig),
        _rebuild       (rebuild),
        _reload        (reload) {
}

void ChunkListCommand::reportError(std::string const& message) {

    LOGS(_log, LOG_LVL_ERROR, "ChunkListCommand::run  " << message);

    proto::WorkerCommandUpdateChunkListR reply;

    reply.set_status(proto::WorkerCommandUpdateChunkListR::ERROR);
    reply.set_error(message);

    _frameBuf.serialize(reply);
    std::string str(_frameBuf.data(), _frameBuf.size());
    _sendChannel->sendStream(xrdsvc::StreamBuffer::createWithMove(str), true);
}

void ChunkListCommand::run() {

    LOGS(_log, LOG_LVL_DEBUG, "ChunkListCommand::run");

    proto::WorkerCommandUpdateChunkListR reply;
    reply.set_status(proto::WorkerCommandUpdateChunkListR::SUCCESS);

    // Rebuild persistent list if requested
    if (_rebuild) {
        ChunkInventory newChunkInventory;
        try {
            xrdsvc::XrdName x;
            newChunkInventory.rebuild(x.getName(), _mySqlConfig);
        } catch (std::exception const& ex) {
            reportError("database operation failed: " + std::string(ex.what()));
            return;
        }
    }

    // Rebuild the transient list and notify the caller if requsted
    if (_reload) {

        // Load the new map from the database into a local variable
        ChunkInventory newChunkInventory;
        try {
            xrdsvc::XrdName x;
            newChunkInventory.init(x.getName(), _mySqlConfig);
        } catch (std::exception const& ex) {
            reportError("database operation failed: " + std::string(ex.what()));
            return;
        }
        ::dumpInventory(*_chunkInventory,  "ChunkListCommand::run  _chunkInventory: ");
        ::dumpInventory(newChunkInventory, "ChunkListCommand::run  newChunkInventory: ");

        // Compare two maps and worker identifiers to see which resources were
        // were added or removed. Then Update the current map and notify XRootD
        // accordingly.

        ChunkInventory::ExistMap const removedChunks = *_chunkInventory  - newChunkInventory;
        ChunkInventory::ExistMap const addedChunks   = newChunkInventory - *_chunkInventory;

        xrdsvc::SsiProviderServer* providerServer = dynamic_cast<xrdsvc::SsiProviderServer*>(XrdSsiProviderLookup);
        XrdSsiCluster*             clusterManager = providerServer->GetClusterManager();

        if (not removedChunks.empty()) {

            for (auto const& entry: removedChunks) {
                std::string const& database = entry.first;

                for (int chunk: entry.second) {
                    std::string const resource = "/chk/" + database + "/" + std::to_string(chunk);

                    LOGS(_log, LOG_LVL_DEBUG, "ChunkListCommand::run  removing resource: " << resource <<
                         " in DataContext=" << clusterManager->DataContext());

                    try {
                        // Notify XRootD/cmsd and (depending on a mode) modify the provider's copy
                        // of the inventory.
                        clusterManager->Removed(resource.c_str());
                        if (clusterManager->DataContext()) {
                            providerServer->GetChunkInventory().remove(database, chunk);
                        }

                        // Notify QServ
                        _chunkInventory->remove(database, chunk);

                    } catch (std::exception const& ex) {
                        reportError("failed to remove the chunk: " + std::string(ex.what()));
                        return;
                    }

                    // Notify the caller of this service
                    proto::WorkerCommandChunk* ptr = reply.add_removed();
                    ptr->set_db(database);
                    ptr->set_chunk(chunk);
                }
            }
        }
        if (not addedChunks.empty()) {

            for (auto const& entry: addedChunks) {
                std::string const& database = entry.first;

                for (int chunk: entry.second) {
                    std::string const resource = "/chk/" + database + "/" + std::to_string(chunk);

                    LOGS(_log, LOG_LVL_DEBUG, "ChunkListCommand::run  adding resource: " << resource <<
                         " in DataContext=" << clusterManager->DataContext());

                    try {
                        // Notify XRootD/cmsd and (depending on a mode) modify the provider's copy
                        // of the inventory.
                        clusterManager->Added(resource.c_str());
                        if (clusterManager->DataContext()) {
                            providerServer->GetChunkInventory().add(database, chunk);
                        }

                        // Notify QServ
                        _chunkInventory->add(database, chunk);

                    } catch (std::exception const& ex) {
                        reportError("failed to add the chunk: " + std::string(ex.what()));
                        return;
                    }

                    // Notify the caller of this service
                    proto::WorkerCommandChunk* ptr = reply.add_added();
                    ptr->set_db(database);
                    ptr->set_chunk(chunk);
                }
            }
        }
    }
    _frameBuf.serialize(reply);
    std::string str(_frameBuf.data(), _frameBuf.size());
    _sendChannel->sendStream(xrdsvc::StreamBuffer::createWithMove(str), true);
}

}}} // namespace lsst::qserv::wpublish
