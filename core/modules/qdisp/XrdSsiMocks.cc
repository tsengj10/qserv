// -*- LSST-C++ -*-
/*
 * LSST Data Management System
 * Copyright 2015 AURA/LSST.
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
 *
 * @author John Gates, SLAC
 */

// System headers
#include <string>
#include <stdlib.h>
#include <unistd.h>

// External headers
#include "boost/thread.hpp"

// lsst headers
#include "lsst/log/Log.h"

// Local headers
#include "qdisp/Executive.h"
#include "qdisp/XrdSsiMocks.h"

using namespace std;

namespace lsst {
namespace qserv {
namespace qdisp {


util::FlagNotify<bool> XrdSsiServiceMock::_go(true);
util::Sequential<int> XrdSsiServiceMock::_count(0);

/** Class to fake being a request to xrootd.
 * Fire up thread that sleeps for a bit and then indicates it was successful.
 */
bool XrdSsiServiceMock::Provision(Resource *resP, unsigned short  timeOut){
    if (resP == NULL) {
        LOGF_ERROR("XrdSsiServiceMock::Provision() invoked with a null Resource pointer.");
        return false;
    }
    lsst::qserv::qdisp::QueryResource *qr = dynamic_cast<lsst::qserv::qdisp::QueryResource*>(resP);
    if (qr == NULL) {
        LOGF_ERROR("XrdSsiServiceMock::Provision() unexpected resource type.");
        return false;
    }
    _count.incr();

    boost::thread t(&XrdSsiServiceMock::mockProvisionTest, this, qr, timeOut);
    // Thread must live past the end of this function, and the calling body
    // is not really dealing with threads, and this is for testing only.
    t.detach();
    return true;
}

/** Mock class for testing Executive.
 * The payload of qr should contain the number of milliseconds this function will sleep before returning.
 */
void XrdSsiServiceMock::mockProvisionTest(qdisp::QueryResource *qr, unsigned short  timeOut){
    string payload = qr->_payload;
    int millisecs = atoi(payload.c_str());
    // barrier for all threads when _go is false.
    _go.wait(true);
    LOGF_INFO("XrdSsiServiceMock::mockProvisionTest sleep begin");
    boost::this_thread::sleep(boost::posix_time::milliseconds(millisecs));
    LOGF_INFO("XrdSsiServiceMock::mockProvisionTest sleep end");
    qr->_status.report(ExecStatus::RESPONSE_DONE);
    qr->_finishFunc->operator ()(true); // This should call class NotifyExecutive::operator()
}

}}} // namespace