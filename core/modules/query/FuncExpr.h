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

#ifndef LSST_QSERV_QUERY_FUNCEXPR_H
#define LSST_QSERV_QUERY_FUNCEXPR_H
/**
  * @file
  *
  * @brief FuncExpr is a SQL function expression including a name and a list of
  * parameters.
  *
  * @author Daniel L. Wang, SLAC
  */

// System headers
#include <memory>
#include <string>

// Local headers
#include "query/ColumnRef.h"
#include "query/typedefs.h"

namespace lsst {
namespace qserv {
namespace query {

// Forward
class QueryTemplate;

// FuncExpr is a function expression, e.g., foo(1,2,bar)
class FuncExpr {
public:
    typedef std::shared_ptr<FuncExpr> Ptr;

    const std::string& getName() const;
    ValueExprPtrVector getParams() const;

    /// Construct a new FuncExpr like an existing one.
    static FuncExpr::Ptr newLike(FuncExpr const& src, std::string const& newName);
    /// Construct a new FuncExpr with a name and string arg
    static FuncExpr::Ptr newArg1(std::string const& newName,
                                 std::string const& arg1);
    /// Construct a new FuncExpr with a name and ValueExpr arg
    static FuncExpr::Ptr newArg1(std::string const& newName,
                                 ValueExprPtr ve);
    /// Construct a new FuncExpr with a name and a vector of ValueExpr arg
    static FuncExpr::Ptr newWithArgs(std::string const& newName,
                                     const ValueExprPtrVector& ve);


    void setName(const std::string& val);

    void findColumnRefs(ColumnRef::Vector& outputRefs);
    std::shared_ptr<FuncExpr> clone() const;

    // Fields
    ValueExprPtrVector params;
    void renderTo(QueryTemplate& qt) const;

    bool operator==(const FuncExpr& rhs) const;
private:
    friend std::ostream& operator<<(std::ostream& os, FuncExpr const& fe);
    friend std::ostream& operator<<(std::ostream& os, FuncExpr const* fe);

    std::string _name;
};


// output helpers
std::ostream& output(std::ostream& os, ValueExprPtrVector const& vel);
void renderList(QueryTemplate& qt, ValueExprPtrVector const& vel);

}}} // namespace lsst::qserv::query

#endif // LSST_QSERV_QUERY_FUNCEXPR_H
