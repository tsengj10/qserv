// -*- LSST-C++ -*-
/*
 * LSST Data Management System
 * Copyright 2012-2016 AURA/LSST.
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
  * @author Daniel L. Wang, SLAC
  */

// Class header
#include "qana/AggregatePlugin.h"

// System headers
#include <string>
#include <stdexcept>

// Third-party headers

// LSST headers
#include "lsst/log/Log.h"

// Qserv headers
#include "qana/CheckAggregation.h"
#include "query/AggOp.h"
#include "query/FuncExpr.h"
#include "query/QueryContext.h"
#include "query/QueryTemplate.h"
#include "query/SelectList.h"
#include "query/SelectStmt.h"
#include "query/ValueExpr.h"
#include "query/ValueFactor.h"
#include "util/common.h"

namespace {
LOG_LOGGER _log = LOG_GET("lsst.qserv.qana.AggregatePlugin");
}

namespace lsst {
namespace qserv {
namespace qana {

inline query::ValueExprPtr
newExprFromAlias(std::string const& alias) {
    std::shared_ptr<query::ColumnRef> cr = std::make_shared<query::ColumnRef>("", "", alias);
    std::shared_ptr<query::ValueFactor> vf;
    vf = query::ValueFactor::newColumnRefFactor(cr);
    return query::ValueExpr::newSimple(vf);
}
/// convertAgg build records for merge expressions from parallel expressions
template <class C>
class convertAgg {
public:
    typedef typename C::value_type T;
    convertAgg(C& pList_, C& mList_, query::AggOp::Mgr& aMgr_)
        : pList(pList_), mList(mList_), aMgr(aMgr_) {}
    void operator()(T const& e) {
        _makeRecord(*e);
    }

private:

    void _makeRecord(query::ValueExpr const& e) {
        std::string origAlias = e.getAlias();

        if (!e.hasAggregation()) {
            // Compute aliases as necessary to protect select list
            // elements so that result tables can be dumped and the
            // columns can be re-referenced in merge queries.
            std::string interName = origAlias;
            // If there is no user alias, the expression is unprotected
            // * cannot be protected--can't alias a set of columns
            // simple column names are already legal column names
            if (origAlias.empty() && !e.isStar() && !e.isColumnRef()) {
                    interName = aMgr.getAggName("PASS");
            }
            query::ValueExprPtr par(e.clone());
            par->setAlias(interName);
            pList.push_back(par);

            if (!interName.empty()) {
                query::ValueExprPtr mer = newExprFromAlias(interName);
                mList.push_back(mer);
                mer->setAlias(origAlias);
            } else {
                // No intermediate name (e.g., *) --> passthrough
                mList.push_back(e.clone());
            }
            return;
        }
        // For exprs with aggregation, we must separate out the
        // expression into pieces.
        // Split the elements of a ValueExpr into its
        // constituent ValueFactors, compute the lists in parallel, and
        // then compute the expression result from the parallel
        // results during merging.
        query::ValueExprPtr mergeExpr = std::make_shared<query::ValueExpr>();
        query::ValueExpr::FactorOpVector& mergeFactorOps = mergeExpr->getFactorOps();
        query::ValueExpr::FactorOpVector const& factorOps = e.getFactorOps();
        for(query::ValueExpr::FactorOpVector::const_iterator i=factorOps.begin();
            i != factorOps.end(); ++i) {
            query::ValueFactorPtr newFactor = i->factor->clone();
            if (newFactor->getType() != query::ValueFactor::AGGFUNC) {
                pList.push_back(query::ValueExpr::newSimple(newFactor));
            } else {
                query::AggRecord r;
                r.orig = newFactor;
                if (!newFactor->getFuncExpr()) {
                    throw std::logic_error("Missing FuncExpr in AggRecord");
                }
                query::AggRecord::Ptr p =
                    aMgr.applyOp(newFactor->getFuncExpr()->getName(), *newFactor);
                if (!p) {
                    throw std::logic_error("Couldn't process AggRecord");
                }
                pList.insert(pList.end(), p->parallel.begin(), p->parallel.end());
                query::ValueExpr::FactorOp m;
                m.factor = p->merge;
                m.op = i->op;
                mergeFactorOps.push_back(m);
            }
        }
        mergeExpr->setAlias(origAlias);
        mList.push_back(mergeExpr);
    }

    C& pList;
    C& mList;
    query::AggOp::Mgr& aMgr;
};


////////////////////////////////////////////////////////////////////////
// AggregatePlugin implementation
////////////////////////////////////////////////////////////////////////
void
AggregatePlugin::applyPhysical(QueryPlugin::Plan& plan,
                               query::QueryContext&  context) {
    // For each entry in original's SelectList, modify the SelectList
    // for the parallel and merge versions.
    // Set hasMerge to true if aggregation is detected.
    query::SelectList& oList = plan.stmtOriginal.getSelectList();

    // Get the first out of the parallel statement select list. Assume
    // that the select lists are the same for all statements. This is
    // not necessarily true, unless the plugin is placed early enough
    // to ensure that other fragmenting activity has not taken place yet.
    query::SelectList& pList = plan.stmtParallel.front()->getSelectList();
    query::SelectList& mList = plan.stmtMerge.getSelectList();
    std::shared_ptr<query::ValueExprPtrVector> vlist;
    vlist = oList.getValueExprList();
    if (!vlist) {
        throw std::invalid_argument("No select list in original SelectStmt");
    }

    // Clear out select lists, since we are rewriting them.
    pList.getValueExprList()->clear();
    mList.getValueExprList()->clear();
    query::AggOp::Mgr m; // Eventually, this can be shared?
    convertAgg<query::ValueExprPtrVector> ca(*pList.getValueExprList(),
                                             *mList.getValueExprList(),
                                             m);
    std::for_each(vlist->begin(), vlist->end(), ca);
    query::QueryTemplate qt;
    pList.renderTo(qt);
    qt.clear();
    mList.renderTo(qt);
    // Also need to operate on GROUP BY.
    // update context.
    if (plan.stmtOriginal.getDistinct() || m.hasAggregate()) {
        context.needsMerge = true;
    }

    std::shared_ptr<query::OrderByClause> _nullptr;

    for(auto first=plan.stmtParallel.begin(), parallel_query=first, end=plan.stmtParallel.end();
        parallel_query != end; ++parallel_query) {

        // Strip ORDER BY from parallel queries if merging.
        if (context.needsMerge && not plan.stmtOriginal.hasLimit()) {
            (*parallel_query)->setOrderBy(_nullptr);
        }

        // Deep-copy select list of first parallel query to all other parallel queries
        // i.e. make the select lists of other statements in the parallel
        // portion the same.
        if (parallel_query != first) {
            (*parallel_query)->setSelectList(pList.clone());
        }
    }
}

}}} // namespace lsst::qserv::qana
