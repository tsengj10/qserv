// -*- LSST-C++ -*-
/*
 * LSST Data Management System
 * Copyright 2013-2016 AURA/LSST.
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
  * @author Daniel L. Wang, SLAC
  */

// Class header
#include "query/JoinSpec.h"

// System headers
#include <stdexcept>

// Third-party headers

// Qserv headers
#include "query/BoolTerm.h"
#include "query/ColumnRef.h"
#include "query/QueryTemplate.h"

namespace lsst {
namespace qserv {
namespace query {

inline bool isInconsistent(JoinSpec const& s) {
    return s.getOn() && s.getUsing();
}

std::ostream& operator<<(std::ostream& os, JoinSpec const& js) {
    os << "JoinSpec(";
    os << "usingColumn:" << js._usingColumn;
    os << ", onTerm:" << js._onTerm;
    os << ")";
    return os;
}

std::ostream& operator<<(std::ostream& os, JoinSpec const* js) {
    (nullptr == js) ? os << "nullptr" : os << *js;
    return os;
}

std::ostream& JoinSpec::putStream(std::ostream& os) const {
    // boilerplate impl until we can think of something better
    QueryTemplate qt;
    putTemplate(qt);
    return os << qt;
}
void JoinSpec::putTemplate(QueryTemplate& qt) const {
    if (isInconsistent(*this)) {
        throw std::logic_error("Inconsistent JoinSpec with ON and USING");
    }
    if (_onTerm) {
        qt.append("ON");
        _onTerm->renderTo(qt);
    } else if (_usingColumn) {
        qt.append("USING");
        qt.append("(");
        qt.append(*_usingColumn); // FIXME: update to support column lists
        qt.append(")");
    } else {
        throw std::logic_error("Empty JoinSpec");
    }
}

JoinSpec::Ptr JoinSpec::clone() const {
    if (isInconsistent(*this)) {
        throw std::logic_error("Can't clone JoinSpec with ON and USING");
    }
    if (_usingColumn) {
        std::shared_ptr<ColumnRef> col = std::make_shared<ColumnRef>(*_usingColumn);
        return std::make_shared<JoinSpec>(col);
    } else {
        return std::make_shared<JoinSpec>(_onTerm->copySyntax());
    }
}


bool JoinSpec::operator==(const JoinSpec& rhs) const {
    return util::ptrCompare<ColumnRef>(_usingColumn, rhs._usingColumn) &&
           util::ptrCompare<BoolTerm>(_onTerm, rhs._onTerm);
}


}}} // lsst::qserv::query
