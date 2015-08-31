// -*- LSST-C++ -*-
/*
 * LSST Data Management System
 * Copyright 2009-2015 AURA/LSST.
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

 /**
  * @file
  *
  * @brief Test functions and structures used in QueryAnalysis tests
  *
  * @author Fabrice Jammes, IN2P3/SLAC
  */

// System headers
#include <algorithm>
#include <iostream>
#include <iterator>
#include <map>
#include <sstream>
#include <string>

// LSST headers
#include "lsst/log/Log.h"

// Qserv headers
#include "parser/SelectParser.h"
#include "qproc/ChunkQuerySpec.h"
#include "qproc/QuerySession.h"
#include "qproc/testMap.h" // Generated by scons action from testMap.kvmap
#include "query/Constraint.h"
#include "query/SelectStmt.h"
#include "tests/QueryAnaHelper.h"

// Boost unit test header
#include "boost/test/included/unit_test.hpp"

using lsst::qserv::parser::SelectParser;
using lsst::qserv::qproc::ChunkQuerySpec;
using lsst::qserv::qproc::ChunkSpec;
using lsst::qserv::qproc::ChunkSpec;
using lsst::qserv::qproc::QuerySession;
using lsst::qserv::query::Constraint;
using lsst::qserv::query::ConstraintVec;
using lsst::qserv::query::ConstraintVector;
using lsst::qserv::util::formatable;

namespace lsst {
namespace qserv {
namespace tests {

struct QueryAnaFixture {

    QueryAnaFixture(void) {
        qsTest.cfgNum = 0;
        qsTest.defaultDb = "LSST";
        // To learn how to dump the map, see qserv/core/css/KvInterfaceImplMem.cc
        // Use admin/examples/testMap_generateMap
        std::string mapBuffer(reinterpret_cast<char const*>(testMap),
                              testMap_length);
        std::istringstream mapStream(mapBuffer);
        std::string emptyChunkPath(".");
        qsTest.cssFacade =
            lsst::qserv::css::FacadeFactory::createMemFacade(mapStream,
                                                             emptyChunkPath);
    };

    QuerySession::Test qsTest;
    QueryAnaHelper queryAnaHelper;
};

}}} // namespace lsst::qserv::test
