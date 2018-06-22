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

/// qserv-replica-jobctrl-test.cc is a simple test application for launching
/// jobs via JobController API.

// System headers
#include <atomic>
#include <iostream>
#include <stdexcept>
#include <string>

// Qserv headers
#include "proto/replication.pb.h"
#include "replica/JobController.h"
#include "replica/ReplicaInfo.h"
#include "replica/FindAllJob.h"
#include "replica/ServiceProvider.h"
#include "util/BlockPost.h"
#include "util/CmdLineParser.h"

using namespace lsst::qserv;

namespace {

// Command line parameters

std::string configUrl;
std::string databaseFamily;
bool        saveReplicaInfo = false;
bool        progressReport;
bool        errorReport;
bool        chunkLocksReport;

/// Run the test
bool test() {

    try {

        ///////////////////////////////////////////////////////////////////////////
        // Start the JobbController in its own thread before ininitating any jobs
        // Note that omFinish callbak which are activated upon a completion
        // of the job will be run in a thread wich will differ from the current one

        replica::ServiceProvider::Ptr const provider  = replica::ServiceProvider::create(configUrl);
        replica::JobController::Ptr   const jobCtrl   = replica::JobController::create(provider);

        jobCtrl->run();

        ////////////////////
        // Start replication

        std::atomic<bool> finished{false};
        auto job = jobCtrl->findAll(
            databaseFamily,
            saveReplicaInfo,
            [&finished] (replica::FindAllJob::Ptr const& job) {
                finished = true;
            }
        );
        if (job) {
            util::BlockPost blockPost(1000,2000);
            while (not finished) {
                blockPost.wait();
            }
        }

        /////////////////////////////////////////
        // Gracefully shutdown the Job Controller

        jobCtrl->stop();

    } catch (std::exception const& ex) {
        std::cerr << ex.what() << std::endl;
    }
    return true;
}
} /// namespace

int main(int argc, const char* const argv[]) {

    // Verify that the version of the library that we linked against is
    // compatible with the version of the headers we compiled against.

    GOOGLE_PROTOBUF_VERIFY_VERSION;

    // Parse command line parameters
    try {
        util::CmdLineParser parser(
            argc,
            argv,
            "\n"
            "Usage:\n"
            "  <database-family> [--config=<url>]\n"
            "                    [--progress-report]\n"
            "                    [--error-report]\n"
            "                    [--chunk-locks-report]\n"
            "\n"
            "Parameters:\n"
            "  <database-family>    - the name of a database family to inspect\n"
            "\n"
            "Flags and options:\n"
            "  --config             - a configuration URL (a configuration file or a set of the database\n"
            "                         connection parameters [ DEFAULT: file:replication.cfg ]\n"
            "  --progress-report    - the flag triggering progress report when executing batches of requests\n"
            "  --error-report       - the flag triggering detailed report on failed requests\n"
            "  --chunk-locks-report - report chunks which are locked\n");

        ::databaseFamily   = parser.parameter<std::string>(1);
        ::configUrl        = parser.option<std::string>("config", "file:replication.cfg");
        ::progressReport   = parser.flag("progress-report");
        ::errorReport      = parser.flag("error-report");
        ::chunkLocksReport = parser.flag("chunk-locks-report");

    } catch (std::exception const& ex) {
        return 1;
    }
    ::test();
    return 0;
}
