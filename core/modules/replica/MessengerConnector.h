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
#ifndef LSST_QSERV_REPLICA_MESSENGERCONNECTOR_H
#define LSST_QSERV_REPLICA_MESSENGERCONNECTOR_H

/// MessengerConnector.h declares:
///
/// class MessengerConnector
/// (see individual class documentation for more information)

// System headers
#include <functional>
#include <list>
#include <memory>
#include <stdexcept>
#include <string>

// Third party headers
#include <boost/asio.hpp>

// Qserv headers
#include "proto/replication.pb.h"
#include "replica/Configuration.h"
#include "replica/ProtocolBuffer.h"
#include "replica/ServiceProvider.h"
#include "util/Mutex.h"

// This header declarations

namespace lsst {
namespace qserv {
namespace replica {

/**
 * Class MessageWrapperBase is the base class for request wrappers.
 */
class MessageWrapperBase {

public:

    /// The pointer type for the base class of the request wrappers
    typedef std::shared_ptr<MessageWrapperBase> Ptr;

    // Default construction and copy semantics are prohibited

    MessageWrapperBase() = delete;
    MessageWrapperBase(MessageWrapperBase const&) = delete;
    MessageWrapperBase& operator=(MessageWrapperBase const&) = delete;

    virtual ~MessageWrapperBase() = default;

    /// @return the completion status to be returned to a subscriber
    bool success() const { return _success; }

    /// @return unique identifier of the request
    std::string const& id() const { return _id; }

    /// @return a pointer onto a buffer with a serialized request
    std::shared_ptr<ProtocolBuffer> const& requestBufferPtr() const { return _requestBufferPtr; }

    ///  @return a non-const reference to buffer for receiving responses from a worker
    ProtocolBuffer& responseBuffer() { return _responseBuffer; }

    /**
     * Update the completion status of a request to 'success'.
     *
     * This method is supposed to be called upon a successful comoletion of a request
     * after a valid response is received from a worker and before notifying
     * a subscriber.
     *
     * @param success - new completion status
     */
    void setSuccess(bool status) { _success = status; }

    /**
     * Parse the content of the buffer and notify a subscriber
     */
    virtual void parseAndNotify()=0;

protected:

    /**
     * Construct the object in the default failed (member 'success') state. Hence, there is no
     * need to set this state explicitly unless a transaction turnes out to be
     * a success.
     *
     * @param id_                         - a unique identifier of the request
     * @param requestBufferPtr_           - an input buffer with seriealized request
     * @param responseBufferCapacityBytes - the initial size of the response buffer
     */
    MessageWrapperBase(std::string const& id_,
                std::shared_ptr<ProtocolBuffer> const& requestBufferPtr,
                size_t responseBufferCapacityBytes)
        :   _success(false),
            _id(id_),
            _requestBufferPtr(requestBufferPtr),
            _responseBuffer(responseBufferCapacityBytes) {
    }

private:

    /// The completion status to be returned to a subscriber
    bool _success;

    /// A unique identifier of the request
    std::string _id;

    /// The buffer with a serialized request
    std::shared_ptr<ProtocolBuffer> _requestBufferPtr;

    /// The buffer for receiving responses from a worker server
    ProtocolBuffer _responseBuffer;
};

/**
 * Class template MessageWrapper extends its based to support type-specific
 * treatment (including serialization) of responses from workers.
 */
template <class RESPONSE_TYPE>
class MessageWrapper
    :   public MessageWrapperBase {

public:

    typedef std::function<void(std::string const&,
                               bool,
                               RESPONSE_TYPE const&)> CallbackType;

    // Default construction and copy semantics are prohibited

    MessageWrapper() = delete;
    MessageWrapper(MessageWrapper const&) = delete;
    MessageWrapper& operator=(MessageWrapper const&) = delete;

    ~MessageWrapper() override = default;

    /**
     * The constructor
     *
     * @param id                          - a unique identifier of the request
     * @param requestBufferPtr            - a request serielized into a network buffer
     * @param responseBufferCapacityBytes - the initial size of the response buffer
     * @param onFinish                    - an asynchronious callback function called upon
     *                                      a completion or failure of the operation
     */
    MessageWrapper(std::string const& id,
                   std::shared_ptr<ProtocolBuffer> const& requestBufferPtr,
                   size_t responseBufferCapacityBytes,
                   CallbackType onFinish)
        :   MessageWrapperBase(id,
                        requestBufferPtr,
                        responseBufferCapacityBytes),
            _onFinish(onFinish) {
    }

    /**
     * @see MessageWrapperBase::parseResponseAndNotify
     */
    void parseAndNotify() override {
        RESPONSE_TYPE response;
        if (success()) {
            try {
                responseBuffer().parse(response, responseBuffer().size());
            } catch(std::runtime_error const& ex) {

                // The message is corrupt. Google Protobuf will report an error
                // of the following kind:
                //
                //   [libprotobuf ERROR google/protobuf/message_lite.cc:123] Can't parse message of type ...
                //
                setSuccess(false);
            }
        }
        _onFinish(id(), success(), response);
    }

private:

    /// The collback fnction to be called upon the completion of the transaction
    CallbackType _onFinish;
};

/**
 * Class MessengerConnector provides a communication interface for sending/receiving
 * messages to and from worker services. It provides connection multiplexing and
 * automatic reconnects.
 *
 * NOTES ON THREAD SAFETY:
 *
 * - in the implementation of this class a mutex is used to prevent race conditions
 *   when performing internal state transitions.
 *
 * - to avoid deadlocks, only externally called methods of the public API (such
 *   as the ones for sending or cancelling requests) and assynchronious callbacks
 *   are locking the mutex. Those methods are NOT allowed to call each other.
 *   Otherwise deadlocks are imminent.
 *
 * - private methods (where a state transition occures or which are relying
 *   on specific states) are required to be called with a reference to
 *   the lock acquired prior to the calls.
 */
class MessengerConnector
    :   public std::enable_shared_from_this<MessengerConnector> {

public:

    /// The pointer type for instances of the class
    typedef std::shared_ptr<MessengerConnector> Ptr;

    // Default construction and copy semantics are prohibited

    MessengerConnector() = delete;
    MessengerConnector(MessengerConnector const&) = delete;
    MessengerConnector& operator=(MessengerConnector const&) = delete;

    ~MessengerConnector() = default;

    /**
     * Create a new connector with specified parameters.
     *
     * Static factory method is needed to prevent issue with the lifespan
     * and memory management of instances created otherwise (as values or via
     * low-level pointers).
     *
     * @param serviceProvider  - a host of services for various communications
     * @param io_service       - the I/O service for communication. The lifespan of
     *                           the object must exceed the one of this instanc.
     * @param worker           - the name of a worker
     *
     * @return pointer to the created object
     */
    static Ptr create(ServiceProvider::Ptr const& serviceProvider,
                      boost::asio::io_service& io_service,
                      std::string const& worker);

    /**
     * Stop operations
     */
    void stop();

    /**
     * Initiate sending a message
     *
     * The response message will be initialized only in case of successfull completion
     * of the transaction. The method may throw exception std::logic_error if
     * the MessangerConnector already has another transaction registered with the same
     * transaction 'id'.
     *
     * @param id                - a unique identifier of a request
     * @param requestBufferPtr  - a request serielized into a network buffer
     * @param onFinish          - an asynchronious callback function called upon a completion
     *                            or failure of the operation
     */
    template <class RESPONSE_TYPE>
    void send(std::string const& id,
              std::shared_ptr<ProtocolBuffer> const& requestBufferPtr,
              typename MessageWrapper<RESPONSE_TYPE>::CallbackType onFinish) {

        sendImpl(
            std::make_shared<MessageWrapper<RESPONSE_TYPE>>(
                id,
                requestBufferPtr,
                _bufferCapacityBytes,
                onFinish));
    }

    /**
     * Cancel an outstanding transaction
     *
     * If this call succeeds there won't be any 'onFinish' callback made
     * as provided to the 'onFinish' method in method 'send'.
     *
     * The method may throw std::logic_error if the Messanger doesn't have
     * a transaction registered with the specified transaction 'id'.
     *
     * @param id  - a unique identifier of a request
     */
    void cancel(std::string const& id);

    /**
     * Return 'true' if the specified requst is known to the Messenger
     *
     * @param id - a unique identifier of a request
     */
    bool exists(std::string const& id) const;

private:

    /**
     * The constructor
     *
     * @see MessengerConnector::create()
     */
    MessengerConnector(ServiceProvider::Ptr const& serviceProvider,
                       boost::asio::io_service& io_service,
                       std::string const& worker);

    /**
     * The actual implementation of the operation 'send'.
     *
     * The method may throw the same exceptions as method 'sent'
     *
     * @param ptr  - a pointer to the request wrapper object
     */
    void sendImpl(MessageWrapperBase::Ptr const& ptr);

    /// State transitions for the connector object
    enum State {
        STATE_INITIAL,      // no communication is happening
        STATE_CONNECTING,   // attempting to connect to a worker service
        STATE_COMMUNICATING // sending or receiving messages
    };

    /// @return the string representation of the connector's state
    static std::string state2string(State state);

    /**
     * Restart the whole operation from scratch.
     *
     * Cancel any asynchronous operation(s) if not in the initial state
     * w/o notifying a subscriber.
     *
     * NOTE: This method is called internally when there is a doubt that
     *       it's possible to do a clean recovery from a failure.
     *
     * @param lock - a lock on a mutex must be acquired before calling this method
     */
    void restart(util::Lock const& lock);

    /**
     * Start resolving the destination worker host & port
     *
     * @param lock - a lock on a mutex must be acquired before calling this method
     *
     */
    void resolve(util::Lock const& lock);

    /**
     * Callback handler for the asynchronious operation
     *
     * @param ec   - error code to be checked
     * @param iter - the host resolver iterator
     */
    void resolved(boost::system::error_code const& ec,
                  boost::asio::ip::tcp::resolver::iterator iter);

    /**
     * Start resolving the destination worker host & port
     *
     * @param lock - a lock on a mutex must be acquired before calling this method
     */
    void connect(util::Lock const& lock,
                 boost::asio::ip::tcp::resolver::iterator iter);

    /**
     * Callback handler for the asynchronious operation upon its
     * successfull completion will trigger a request-specific
     * protocol sequence.
     *
     * @param ec   - error code to be checked
     * @param iter - the host resolver iterator
     */
    void connected(boost::system::error_code const& ec,
                   boost::asio::ip::tcp::resolver::iterator iter);

    /**
     * Start a timeout before attempting to restart the connection
     * 
     * @param lock - a lock on a mutex must be acquired before calling this method
     */
    void waitBeforeRestart(util::Lock const& lock);

    /**
     * Callback handler fired for restarting the connection
     *
     * @param ec - error code to be checked
     */
    void awakenForRestart(boost::system::error_code const& ec);

    /**
     * Lookup for the next available request and begin sending it
     * unless there is another ongoing request at a time of the call.
     * 
     * @param lock - a lock on a mutex must be acquired before calling this method
     */
    void sendRequest(util::Lock const& lock);

    /**
     * Callback handler fired upon a completion of the request sending
     *
     * @param ec                 - error code to be checked
     * @param bytes_transferred  - the numner of bytes sent
     */
    void requestSent(boost::system::error_code const& ec,
                     size_t bytes_transferred);

    /**
     * Begin receiving a response
     * 
     * @param lock - a lock on a mutex must be acquired before calling this method
     */
    void receiveResponse(util::Lock const& lock);

    /**
     * Callback handler fired upon a completion of the response receiving
     *
     * @param ec                 - error code to be checked
     * @param bytes_transferred  - the numner of bytes sent
     */
    void responseReceived(boost::system::error_code const& ec,
                          size_t bytes_transferred);

    /**
     * Synchroniously read a protocol frame which carries the length
     * of a subsequent message and return that length along with the completion
     * status of the operation.
     *
     * @param lock  - a lock on a mutex must be acquired before calling this method
     * @param buf   - the buffer to use
     * @param bytes - the length in bytes extracted from the frame
     *
     * @return the completion code of the operation
     */
    boost::system::error_code syncReadFrame(util::Lock const& lock,
                                            ProtocolBuffer& buf,
                                            size_t& bytes);

   /**
     * Synchriniously read a response header of a known size. Then parse it
     * and analyze it to ensure its content matches expecations. Return
     * the completion status of the operation.
     *
     * The method will throw exception std::logic_error if the header's
     * content won't match expectations.
     *
     * @param lock  - a lock on a mutex must be acquired before calling this method
     * @param buf   - the buffer to use
     * @param bytes - a expected length of the message (obtained from a preceeding frame)
     *                to be received into the network buffer from the network.
     * @param id    - a unique identifier of a request to match the 'id' in a response header
     *
     * @return the completion code of the operation
     */
    boost::system::error_code syncReadVerifyHeader(util::Lock const& lock,
                                                   ProtocolBuffer& buf,
                                                   size_t bytes,
                                                   std::string const& id);

    /**
     * Synchriniously read a message of a known size into the specified buffer.
     * Return the completion status of the operation. After the successfull
     * completion of the operation the content of the network buffer can be parsed.
     *
     * @param lock  - a lock on a mutex must be acquired before calling this method
     * @param buf   - the buffer to use
     * @param bytes - a expected length of the message (obtained from a preceeding frame)
     *                to be received into the network buffer from the network.
     *
     * @return the completion code of the operation
     */
    boost::system::error_code syncReadMessageImpl(util::Lock const& lock,
                                                  ProtocolBuffer& buf,
                                                  size_t bytes);

    /**
     * Return 'true' if the operation was aborted.
     *
     * USAGE NOTES:
     *
     *    Nomally this method is supposed to be called as the first action
     *    within asynchronous handlers to figure out if an on-going aynchronous
     *    operation was cancelled for some reason. Should this be the case
     *    the caller is supposed to quit right away. It will be up to a code
     *    which initiated the abort to take care of putting the object into
     *    a proper state.
     * 
     * @param ec - error code to be checked
     */
    bool isAborted(boost::system::error_code const& ec) const;

    /// @return the worker-specific context string
    std::string context() const;

    /**
     * Find a request matching the specified identifier
     *
     * @param lock  - a lock on a mutex must be acquired before calling this method
     * @param id    - an identifier of the request
     *
     * @return pointer to the request if found, or an empty pointer otherwise
     */
    MessageWrapperBase::Ptr find(util::Lock const& lock,
                                 std::string const& id) const;

private:

    ServiceProvider::Ptr _serviceProvider;

    /// Cached worker descriptor obtained from the configuration
    WorkerInfo _workerInfo;

    /// The cached parameter for the buffer sizes (pulled from
    /// the Configuration upon the construction of the object).
    size_t _bufferCapacityBytes;

    /// The cached parameter for the interval between reconnection
    /// attempts (pulled from the Configuration upon the construction of
    /// the object).
    unsigned int _timerIvalSec;

    /// The internal state
    State _state;

    boost::asio::ip::tcp::resolver _resolver;
    boost::asio::ip::tcp::socket   _socket;
    boost::asio::deadline_timer    _timer;

    /// This mutex is meant to avoid race conditions to the internal data
    /// structure between a thread which runs the Ntework I/O service
    /// and threads submitting requests.
    mutable util::Mutex _mtx;

    /// The queue (FIFO) of requests
    std::list<MessageWrapperBase::Ptr> _requests;

    /// The currently processed (being sent) request (if any, otherwise
    /// the pointer is set to nullptr)
    MessageWrapperBase::Ptr _currentRequest;

    /// The intermediate buffer for messages received from a worker
    ProtocolBuffer _inBuffer;
};

}}} // namespace lsst::qserv::replica

#endif // LSST_QSERV_REPLICA_MESSENGERCONNECTOR_H
