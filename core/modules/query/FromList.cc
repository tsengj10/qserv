// -*- LSST-C++ -*-
/*
 * LSST Data Management System
 * Copyright 2013-2017 AURA/LSST.
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
  * @brief Implementation of FromList
  *
  * @author Daniel L. Wang, SLAC
  */

// Class header
#include "query/FromList.h"

// System headers
#include <algorithm>
#include <iterator>

#include "util/IterableFormatter.h"


#include "util/PointerCompare.h"

namespace lsst {
namespace qserv {
namespace query {

bool
FromList::isJoin() const {
    if (_tableRefs) {
        int count = 0;
        typedef TableRefList::const_iterator Iter;
        for(Iter i=_tableRefs->begin(), e=_tableRefs->end();
            i != e;
            ++i) {

            if (*i) {
                if ((**i).isSimple()) { ++count; }
            } else {
                count += 2;
            }
            if (count > 1) { return true; }
        }
    }
    return false;
}

std::vector<DbTablePair>
FromList::computeResolverTables() const {
    struct Memo : public TableRef::FuncC {
        virtual void operator()(TableRef const& t) {
            vec.push_back(DbTablePair(t.getDb(), t.getTable()));
        }
        std::vector<DbTablePair> vec;
    };
    Memo m;
    typedef TableRefList::const_iterator Iter;
    for(Iter i=_tableRefs->begin(), e= _tableRefs->end();
        i != e; ++i) {
        (**i).apply(m);
    }
    return m.vec;
}

std::string
FromList::getGenerated() {
    QueryTemplate qt;
    renderTo(qt);
    return qt.sqlFragment();
}

void
FromList::renderTo(QueryTemplate& qt) const {
    if (_tableRefs != nullptr && _tableRefs->size() > 0) {
        TableRef::render rend(qt);
        for (auto& tRef : *_tableRefs) {
            rend.applyToQT(tRef);
        }
    }
}

std::shared_ptr<FromList>
FromList::copySyntax() {
    std::shared_ptr<FromList> newL = std::make_shared<FromList>(*this);
    // Shallow copy of expr list is okay.
    newL->_tableRefs  = std::make_shared<TableRefList>(*_tableRefs);
    // For the other fields, default-copied versions are okay.
    return newL;
}

std::shared_ptr<FromList>
FromList::clone() const {
    typedef TableRefList::const_iterator Iter;
    std::shared_ptr<FromList> newL = std::make_shared<FromList>(*this);

    newL->_tableRefs = std::make_shared<TableRefList>();

    for(Iter i=_tableRefs->begin(), e=_tableRefs->end(); i != e; ++ i) {
        newL->_tableRefs->push_back((*i)->clone());
    }
    return newL;
}

bool FromList::operator==(const FromList& rhs) {
    return util::ptrVectorPtrCompare<TableRef>(_tableRefs, rhs._tableRefs);
}

std::ostream& operator<<(std::ostream& os, FromList const& fromList) {
    os << "FromList(tableRefs:";
    if (nullptr == fromList._tableRefs) {
        os << "nullptr";
    } else {
        os << util::printable(*fromList._tableRefs);
    }
    os << ")";
    return os;
}

std::ostream& operator<<(std::ostream& os, FromList const* fromList) {
    (nullptr == fromList) ? os << "nullptr" : os << *fromList;
    return os;
}



}}} // namespace lsst::qserv::query

