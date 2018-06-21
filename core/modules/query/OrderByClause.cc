// -*- LSST-C++ -*-
/*
 * LSST Data Management System
 * Copyright 2012-2017 AURA/LSST.
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
  * @brief Implementation of OrderByTerm and OrderByClause
  *
  * @author Daniel L. Wang, SLAC
  */

// Class header
#include "query/OrderByClause.h"

// System headers
#include <iostream>
#include <iterator>
#include <sstream>

// Third-party headers

// LSST headers
#include "lsst/log/Log.h"

// Qserv headers
#include "query/QueryTemplate.h"
#include "query/ValueExpr.h"
#include "util/PointerCompare.h"
#include "util/DbgPrintPtrH.h"

namespace {

LOG_LOGGER _log = LOG_GET("lsst.qserv.query.OrderByClause");

using lsst::qserv::query::OrderByTerm;

char const* getOrderStr(OrderByTerm::Order o) {
    switch(o) {
    case OrderByTerm::ASC: return "ASC";
    case OrderByTerm::DESC: return "DESC";
    case OrderByTerm::DEFAULT: return "";
    default: return "UNKNOWN_ORDER";
    }
}

} // anonymous namespace

namespace lsst {
namespace qserv {
namespace query {

class OrderByTerm::render : public std::unary_function<OrderByTerm, void> {
public:
    render(QueryTemplate& qt) : _qt(qt), _count(0) {}
    void applyToQT(OrderByTerm const& term) {
        if (_count++ > 0) {
            _qt.append(", ");
        }
        term.renderTo(_qt);
        LOGS(_log, LOG_LVL_TRACE, "Query Template: " << _qt);
    }
    QueryTemplate& _qt;
    int _count;
};

////////////////////////////////////////////////////////////////////////
// OrderByTerm
////////////////////////////////////////////////////////////////////////
void
OrderByTerm::renderTo(QueryTemplate& qt) const {
    ValueExpr::render r(qt, true);
    r.applyToQT(_expr);
    if (!_collate.empty()) {
        qt.append("COLLATE");
        qt.append(_collate);
    }
    char const* orderStr = getOrderStr(_order);
    if (orderStr && orderStr[0] != '\0') {
        qt.append(orderStr);
    }
}

std::string OrderByTerm::sqlFragment() const {
    std::ostringstream oss;
    oss << *this;
    return oss.str();
}

std::ostream&
operator<<(std::ostream& os, OrderByTerm const& t) {
    os << *(t._expr);
    if (!t._collate.empty()) os << " COLLATE " << t._collate;
    char const* orderStr = getOrderStr(t._order);
    if (orderStr && orderStr[0] != '\0') {
        os << " " << orderStr;
    }
    return os;
}

bool OrderByTerm::operator==(const OrderByTerm& rhs) const {
    return util::pointerCompare(_expr, rhs._expr) &&
            _order == rhs._order &&
            _collate == rhs._collate;
}

void OrderByTerm::dbgPrint(std::ostream& os) const {
    os << "OrderByTerm(";
    os << "expr:" << util::DbgPrintPtrH<ValueExpr>(_expr);
    os << ", order:";
    switch (_order) {
    case DEFAULT: os << "DEFAULT"; break;
    case ASC: os << "ASC"; break;
    case DESC: os << "DESC"; break;
    default: os << "!!unhandled!!"; break;
    }
    os << ", collate:" <<  _collate;
    os << ")";
}

////////////////////////////////////////////////////////////////////////
// OrderByClause
////////////////////////////////////////////////////////////////////////
std::ostream&
operator<<(std::ostream& out, OrderByClause const& clause) {
    if (clause._terms) {
        auto const& terms = *(clause._terms);
        if (not terms.empty()) {
            out << "ORDER BY ";
            if (terms.size() > 1) {
                std::ostream_iterator<OrderByTerm> string_it(out, ", ");
                std::copy(terms.begin(), terms.end()-1, string_it);
            }
            out << terms.back();
        }
    }
    return out;
}

std::string OrderByClause::sqlFragment() const {
    std::ostringstream oss;
    oss << *this;
    return oss.str();
}

void
OrderByClause::renderTo(QueryTemplate& qt) const {
    if (_terms.get() && _terms->size() > 0) {
        OrderByTerm::render r(qt);
        for(auto& term : *_terms) {
            LOGS(_log, LOG_LVL_TRACE, "Rendering term: " << term);
            r.applyToQT(term);
        }
    }
}

std::shared_ptr<OrderByClause> OrderByClause::clone() const {
    return std::make_shared<OrderByClause>(*this); // FIXME
}
std::shared_ptr<OrderByClause> OrderByClause::copySyntax() {
    return std::make_shared<OrderByClause>(*this);
}

void OrderByClause::findValueExprs(ValueExprPtrVector& list) {
    for (OrderByTermVector::iterator i = _terms->begin(), e = _terms->end(); i != e; ++i) {
        list.push_back(i->getExpr());
    }
}

bool OrderByClause::operator==(const OrderByClause& rhs) const {
    return util::pointerCompare(_terms, rhs._terms);
}

void OrderByClause::dbgPrint(std::ostream& os) const {
    os << "OrderByClause(terms:" << util::DbgPrintPtrVectorH<OrderByTerm>(_terms) << ")";
}

}}} // namespace lsst::qserv::query
