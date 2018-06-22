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

// Class header
#include "wpublish/QservRequest.h"

// System headers
#include <string>

// Qserv headers
#include "lsst/log/Log.h"

namespace {

LOG_LOGGER _log = LOG_GET("lsst.qserv.wpublish.QservRequest");

}  // namespace

namespace lsst {
namespace qserv {
namespace wpublish {

// Set this parameter to some reasonable default
int const QservRequest::_bufIncrementSize = 1024;

QservRequest::~QservRequest() {
    delete _buf;
}

QservRequest::QservRequest()
    :    _bufSize(0),
         _bufCapacity(_bufIncrementSize),
         _buf(new char[_bufIncrementSize]) {
}

char* QservRequest::GetRequest(int& dlen) {

    // Ask a subclass to serialize its request into the frame buffer
    onRequest(_frameBuf);

    // Tell SSI which data and how many bytes to send
    dlen = _frameBuf.size();
    return _frameBuf.data();
}

bool QservRequest::ProcessResponse(const XrdSsiErrInfo&  eInfo,
                                   const XrdSsiRespInfo& rInfo) {

    static std::string const context = "QservRequest::ProcessResponse  ";

    if (eInfo.hasError()) {

        // Copy the argument before sending the upstream notification
        // Otherwise the current object may get disposed before we even had
        // a chance to notify XRootD/SSI by calling Finished().
        std::string const errorStr = rInfo.eMsg;

        LOGS(_log, LOG_LVL_ERROR, context << "** FAILED **, error: " << errorStr);

        // Tell XrootD to realease all resources associated with this request
        Finished();

        // Notify a subclass on the ubnormal condition
        // WARNING: This has to be the last call as the object may get deleted
        //          downstream.
        onError(errorStr);

        return false;
    }
    LOGS(_log, LOG_LVL_DEBUG, context
        << "  eInfo.rType: " << rInfo.rType << "(" << rInfo.State() << ")"
        << ", eInfo.blen: " << rInfo.blen);

    switch (rInfo.rType) {

        case XrdSsiRespInfo::isData:
        case XrdSsiRespInfo::isStream:

            LOGS(_log, LOG_LVL_DEBUG, context << "** REQUESTING RESPONSE DATA **");
            GetResponseData(_buf + _bufSize, _bufIncrementSize);
            return true;

        default:

            // Copy the argument before sending the upstream notification
            // Otherwise the current object may get disposed before we even had
            // a chance to notify XRootD/SSI by calling Finished().
            std::string const responseType = std::to_string(rInfo.rType);

            // Tell XrootD to realease all resources associated with this request
            Finished();

            // Notify a subclass on the ubnormal condition
            // WARNING: This has to be the last call as the object may get deleted
            //          downstream.
            onError("QservRequest::ProcessResponse  ** ERROR ** unexpeted response type: " + responseType);
            return false;
    }
}

XrdSsiRequest::PRD_Xeq QservRequest::ProcessResponseData(const XrdSsiErrInfo& eInfo,
                                                         char* buff,
                                                         int   blen,
                                                         bool  last) {

    static std::string const context = "QservRequest::ProcessResponseData  ";

    LOGS(_log, LOG_LVL_DEBUG, context << "eInfo.isOK: " << eInfo.isOK());

    if (not eInfo.isOK()) {

        // Copy these arguments before sending the upstream notification.
        // Otherwise the current object may get disposed before we even had
        // a chance to notify XRootD/SSI by calling Finished().

        std::string const errorStr = eInfo.Get();
        int         const errorNum = eInfo.GetArg();

        LOGS(_log, LOG_LVL_ERROR, context << "** FAILED **  eInfo.Get(): " << errorStr
             << ", eInfo.GetArg(): " << errorNum);

         // Tell XrootD to realease all resources associated with this request
         Finished();

        // Notify a subclass on the ubnormal condition.
        // WARNING: This has to be the last call as the object may get deleted
        //          downstream.
        onError(errorStr);

    } else {
        LOGS(_log, LOG_LVL_DEBUG, context << "blen: " << blen << ", last: " << last);

        // Update the byte counter
        _bufSize += blen;

        if (last) {

            // Tell XrootD to realease all resources associated with this request
            Finished();

            // Ask a subclass to process the response
            // WARNING: This has to be the last call as the object may get deleted
            //          downstream.
            proto::FrameBufferView view(_buf, _bufSize);
            onResponse(view);

        } else {
            // Extend the buffer and copy over its previous content into the new location
            int prevBufCapacity = _bufCapacity;
            _bufCapacity += _bufIncrementSize;

            char* prevBuf = _buf;
            _buf = new char[_bufCapacity];

            std::copy(prevBuf, prevBuf + prevBufCapacity, _buf);

            // Keep reading
            GetResponseData(_buf + _bufSize, _bufIncrementSize);
        }
    }
    return XrdSsiRequest::PRD_Normal;
}

}}} // namespace lsst::qserv::wpublish
