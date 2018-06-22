// -*- LSST-C++ -*-
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
#ifndef LSST_QSERV_REPLICA_WORKERREPLICATIONREQUEST_H
#define LSST_QSERV_REPLICA_WORKERREPLICATIONREQUEST_H

/// WorkerReplicationRequest.h declares:
///
/// class WorkerReplicationRequest
/// class WorkerReplicationRequestPOSIX
/// class WorkerReplicationRequestX
/// (see individual class documentation for more information)

// System headers
#include <cstdio>               // std::FILE, C-style file I/O
#include <ctime>
#include <map>
#include <string>

// Tird party headers
#include <boost/filesystem.hpp>

// Qserv headers
#include "replica/Configuration.h"
#include "replica/ReplicaInfo.h"
#include "replica/WorkerRequest.h"

// Forward declarations

// This header declarations

namespace lsst {
namespace qserv {
namespace replica {

// Forward declarations
class FileClient;

/**
  * Class WorkerReplicationRequest represents a context and a state of replication
  * requsts within the worker servers. It can also be used for testing the framework
  * operation as its implementation won't make any changes to any files or databases.
  *
  * Real implementations of the request processing must derive from this class.
  */
class WorkerReplicationRequest
    :   public WorkerRequest {

public:

    /// Pointer to self
    typedef std::shared_ptr<WorkerReplicationRequest> Ptr;

    /**
     * Static factory method is needed to prevent issue with the lifespan
     * and memory management of instances created otherwise (as values or via
     * low-level pointers).
     * 
     * @param serviceProvider  - a host of services for various communications
     * @param worker           - the name of a worker
     * @param id               - an identifier of a client request
     * @param priority         - indicates the importance of the request
     * @param database         - the name of a database
     * @param chunk            - the chunk number
     * @param sourceWorker     - the name of a source worker
     *
     * @return pointer to the created object
     */
    static Ptr create(ServiceProvider::Ptr const& serviceProvider,
                      std::string const& worker,
                      std::string const& id,
                      int priority,
                      std::string const& database,
                      unsigned int chunk,
                      std::string const& sourceWorker);

    // Default construction and copy semantics are prohibited

    WorkerReplicationRequest() = delete;
    WorkerReplicationRequest(WorkerReplicationRequest const&) = delete;
    WorkerReplicationRequest& operator=(WorkerReplicationRequest const&) = delete;

    /// Destructor
    ~WorkerReplicationRequest() override = default;

    // Trivial accessors

    std::string const& database() const { return _database; }

    unsigned int chunk() const { return _chunk; }

    std::string const& sourceWorker() const { return _sourceWorker; }

    /**
     * Extract request status into the Protobuf response object.
     *
     * @param response - Protobuf response to be initialized
     */
    void setInfo(proto::ReplicationResponseReplicate& response) const;

    /**
     * @see WorkerRequest::execute
     */
    bool execute() override;

protected:

    /**
     * The normal constructor of the class
     *
     * @see WorkerReplicationRequest::created()
     */
    WorkerReplicationRequest(ServiceProvider::Ptr const& serviceProvider,
                             std::string const& worker,
                             std::string const& id,
                             int priority,
                             std::string const& database,
                             unsigned int chunk,
                             std::string const& sourceWorker);

protected:

    std::string  _database;
    unsigned int _chunk;
    std::string  _sourceWorker;

    /// Result of the operation
    ReplicaInfo _replicaInfo;
};

/**
  * Class WorkerReplicationRequestPOSIX provides an actual implementation for
  * the replication requests based on the direct manipulation of files on
  * a POSIX file system.
  */
class WorkerReplicationRequestPOSIX
    :   public WorkerReplicationRequest {

public:

    /// Pointer to self
    typedef std::shared_ptr<WorkerReplicationRequestPOSIX> Ptr;

    /**
     * Static factory method is needed to prevent issue with the lifespan
     * and memory management of instances created otherwise (as values or via
     * low-level pointers).
     *
     * @param serviceProvider  - a host of services for various communications
     * @param worker           - the name of a worker
     * @param id               - an identifier of a client request
     * @param priority         - indicates the importance of the request
     * @param database         - the name of a database
     * @param chunk            - the chunk number
     * @param sourceWorker     - the name of a source worker
     *
     * @return pointer to the created object
     */
    static Ptr create(ServiceProvider::Ptr const& serviceProvider,
                      std::string const& worker,
                      std::string const& id,
                      int priority,
                      std::string const& database,
                      unsigned int chunk,
                      std::string const& sourceWorker);

    // Default construction and copy semantics are prohibited

    WorkerReplicationRequestPOSIX() = delete;
    WorkerReplicationRequestPOSIX(WorkerReplicationRequestPOSIX const&) = delete;
    WorkerReplicationRequestPOSIX& operator=(WorkerReplicationRequestPOSIX const&) = delete;

    ~WorkerReplicationRequestPOSIX() override = default;

    /**
     * @see WorkerReplicationRequest::execute
     */
    bool execute() override;

protected:

    /**
     * The normal constructor of the class
     *
     * @see WorkerReplicationRequestPOSIX::created()
     */
    WorkerReplicationRequestPOSIX(ServiceProvider::Ptr const& serviceProvider,
                                  std::string const& worker,
                                  std::string const& id,
                                  int priority,
                                  std::string const& database,
                                  unsigned int chunk,
                                  std::string const& sourceWorker);
};

/**
  * Class WorkerReplicationRequestFS provides an actual implementation for
  * the replication requests based on the direct manipulation of local files
  * on a POSIX file system and for reading remote files using the built-into-worker
  * simple file server.
  */
class WorkerReplicationRequestFS
    :   public WorkerReplicationRequest {

public:

    /// Pointer to self
    typedef std::shared_ptr<WorkerReplicationRequestFS> Ptr;

    /**
     * Static factory method is needed to prevent issue with the lifespan
     * and memory management of instances created otherwise (as values or via
     * low-level pointers).
     *
     * @param serviceProvider  - a host of services for various communications
     * @param worker           - the name of a worker
     * @param id               - an identifier of a client request
     * @param priority         - indicates the importance of the request
     * @param database         - the name of a database
     * @param chunk            - the chunk number
     * @param sourceWorker     - the name of a source worker
     *
     * @return pointer to the created object
     */
    static Ptr create(ServiceProvider::Ptr const& serviceProvider,
                      std::string const& worker,
                      std::string const& id,
                      int priority,
                      std::string const& database,
                      unsigned int chunk,
                      std::string const& sourceWorker);

    // Default construction and copy semantics are prohibited

    WorkerReplicationRequestFS() = delete;
    WorkerReplicationRequestFS(WorkerReplicationRequestFS const&) = delete;
    WorkerReplicationRequestFS& operator=(WorkerReplicationRequestFS const&) = delete;

    /// Destructor (non trivial one is needed to release resources)
    ~WorkerReplicationRequestFS() override;

    /**
     * @see WorkerReplicationRequest::execute
     */
    bool execute() override;

protected:

    /**
     * The normal constructor of the class
     *
     * @see WorkerReplicationRequestFS::create()
     */
    WorkerReplicationRequestFS(ServiceProvider::Ptr const& serviceProvider,
                               std::string const& worker,
                               std::string const& id,
                               int priority,
                               std::string const& database,
                               unsigned int chunk,
                               std::string const& sourceWorker);

private:
    
    /**
     * Open files associated with the current state of iterator _fileItr.
     *
     * @param lock - lock which must be acquired before calling this method
     *
     * @return 'false' in case of any error
     */
    bool openFiles(util::Lock const& lock);

    /**
     * The final stage to be executed just once after copying the content
     * of the remote files into the local temporary ones. It will rename
     * the temporary files into the standard ones.
     *
     * Resources will also be released.
     *
     * @param lock - lock which must be acquired before calling this method
     *
     * @return always 'true'
     */
    bool finalize(util::Lock const& lock);

    /**
     * Close connections, deallocate resources, etc.
     *
     * Any connections and open files will be closed, the buffers will be
     * released to prevent unneccesary resource utilization. Note that
     * request objects can stay in the server's memory for an extended
     * period of time.
     *
     * @param lock - lock which must be acquired before calling this method
     */
    void releaseResources(util::Lock const& lock);

    /**
     * Update file migration statistics
     *
     * @param lock - lock which must be acquired before calling this method
     */
    void updateInfo(util::Lock const& lock);

private:

    // Cached descriptors obtained from the Configuration

    WorkerInfo _inWorkerInfo;
    WorkerInfo _outWorkerInfo;

    DatabaseInfo _databaseInfo;

    /// The flag indicating if the initialization phase of the operation
    /// has alreadty completed
    bool _initialized;

    /// Short names of files to be copied
    std::vector<std::string> const _files;

    /// The iterator pointing to the currently processed file.
    /// If it's set to _files.end() then it means the operation
    /// has finished.
    std::vector<std::string>::const_iterator _fileItr;

    /// This object represents the currently open (if any) input file
    /// on the source worker node
    std::shared_ptr<FileClient> _inFilePtr;

    /// The file pointer for the temporary output file
    std::FILE* _tmpFilePtr;

    /// The FileDescr struct encapsulates various parameters of a file
    struct FileDescr {

        /// The input file size as reported by a remote server
        size_t inSizeBytes;

        /// The actual number of bytes read so far (changes as the operation
        /// is progressing)
        size_t outSizeBytes;
        
        /// The last modification time of the file (seconds since UNISX Epoch)
        std::time_t mtime;

        /// Control sum computed locally while copying the file
        uint64_t cs;

        /// The absolute path of a temporary file at a local directory.
        boost::filesystem::path tmpFile;

        /// The final (canonic) file name the temporary file will be renamed intp
        /// upon a successfull completion of the operation.
        boost::filesystem::path outFile;

        /// When the file transfer started
        uint64_t beginTransferTime;

        /// When the file transfer ended
        uint64_t endTransferTime;
    };

    /// Cached file descriptions mapping from short file names into
    /// the corresponidng parameters
    std::map<std::string,FileDescr> _file2descr;

    /// The buffer for storying file payload read from a remote file service
    uint8_t* _buf;

    /// The size of the buffer
    size_t _bufSize;
};


}}} // namespace lsst::qserv::replica

#endif // LSST_QSERV_REPLICA_WORKERREPLICATIONREQUEST_H
