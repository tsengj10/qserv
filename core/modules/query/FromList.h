// -*- LSST-C++ -*-
/*
 * LSST Data Management System
 * Copyright 2012-2015 LSST Corporation.
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

#ifndef LSST_QSERV_QUERY_FROMLIST_H
#define LSST_QSERV_QUERY_FROMLIST_H
/**
  * @file
  *
  * @author Daniel L. Wang, SLAC
  */

// System headers

// Third-party headers
#include <memory>

// Local headers
#include "query/TableRef.h"


namespace lsst {
namespace qserv {

namespace parser {
    // Forward
    class FromFactory;
}

namespace query {

// FromList is a representation of SQL FROM.
class FromList {
public:
    typedef std::shared_ptr<FromList> Ptr;
    explicit FromList(TableRefListPtr p) : _tableRefs(p) {}
    ~FromList() {}
    /// @return a list of TableRef that occur
    TableRefList& getTableRefList() { return *_tableRefs; }
    /// @return a list of TableRef that occur
    TableRefList const& getTableRefList() const { return *_tableRefs; }

    bool isJoin() const;
    std::vector<DbTablePair> computeResolverTables() const;

    /// @return a flattened string representation.
    std::string getGenerated();
    void renderTo(QueryTemplate& qt) const;

    /// Deep-copy this node
    std::shared_ptr<FromList> clone() const;
    /// Shallow copy this node, sharing its linked objects.
    std::shared_ptr<FromList> copySyntax();

    bool operator==(const FromList& rhs);

private:
    friend std::ostream& operator<<(std::ostream& os, FromList const& fromList);
    friend std::ostream& operator<<(std::ostream& os, FromList const* fromList);
    friend class parser::FromFactory;

    TableRefListPtr _tableRefs;
};

}}} // namespace lsst::qserv::query

#endif // LSST_QSERV_QUERY_FROMLIST_H
