// -*- LSST-C++ -*-
/*
 * LSST Data Management System
 * Copyright 2015-2017 AURA/LSST.
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

#ifndef LSST_QSERV_CCONTROL_USERQUERYFACTORY_H
#define LSST_QSERV_CCONTROL_USERQUERYFACTORY_H
/**
  * @file
  *
  * @brief Factory for UserQuery.
  *
  * @author Daniel L. Wang, SLAC
  */

// System headers
#include <cstdint>
#include <memory>

// Third-party headers
#include "boost/utility.hpp"

#include "qdisp/QdispPool.h"
// Local headers
#include "ccontrol/UserQuery.h"
#include "global/stringTypes.h"

namespace lsst {
namespace qserv {

namespace czar {
class CzarConfig;
}

namespace ccontrol {

///  UserQueryFactory breaks construction of user queries into two phases:
///  creation/configuration of the factory and construction of the
///  UserQuery. This facilitates re-use of initialized state that is usually
///  constant between successive user queries.
class UserQueryFactory : private boost::noncopyable {
public:

    UserQueryFactory(czar::CzarConfig const& czarConfig,
                     std::string const& czarName);

    /// @param query:        Query text
    /// @param defaultDb:    Default database name, may be empty
    /// @param qdispPool:    Thread pool handling qdisp jobs.
    /// @param userQueryId:  Unique string identifying query
    /// @param msgTableName: Name of the message table without database name.
    /// @return new UserQuery object
    UserQuery::Ptr newUserQuery(std::string const& query,
                                std::string const& defaultDb,
                                qdisp::QdispPool::Ptr const& qdispPool,
                                std::string const& userQueryId,
                                std::string const& msgTableName);

private:
    class Impl;
    std::shared_ptr<Impl> _impl;
};

}}} // namespace lsst::qserv:control

#endif // LSST_QSERV_CCONTROL_USERQUERYFACTORY_H
