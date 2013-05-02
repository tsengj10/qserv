/*
 * LSST Data Management System
 * Copyright 2013 LSST Corporation.
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

/// \file
/// \brief A tool for estimating the chunk and sub-chunk record
///        counts for the data-sets generated by the Qserv duplicator.

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "boost/filesystem.hpp"
#include "boost/program_options.hpp"
#include "boost/shared_ptr.hpp"

#include "Chunker.h"
#include "ChunkIndex.h"
#include "CmdLineUtils.h"
#include "Geometry.h"
#include "HtmIndex.h"

using std::cerr;
using std::cout;
using std::endl;
using std::exception;
using std::max;
using std::min;
using std::runtime_error;
using std::string;
using std::vector;

using boost::shared_ptr;

namespace fs = boost::filesystem;
namespace po = boost::program_options;


namespace lsst { namespace qserv { namespace admin { namespace dupr {

void defineOptions(po::options_description & opts) {
    po::options_description dup("\\________________ Duplication", 80);
    dup.add_options()
        ("sample.fraction", po::value<double>()->default_value(1.0),
         "The fraction of input positions to include in the output.")
        ("index", po::value<string>(),
         "HTM index file name for the data set to duplicate. May be "
         "omitted, in which case --part.index is used as the HTM index "
         "for both the input data set and for partitioning positions.")
        ("ra-min", po::value<double>()->default_value(0.0),
         "Minimum right ascension bound (deg) for the duplication region.")
        ("ra-max", po::value<double>()->default_value(360.0),
         "Maximum right ascension bound (deg) for the duplication region.")
        ("dec-min", po::value<double>()->default_value(-90.0),
         "Minimum declination bound (deg) for the duplication region.")
        ("dec-max", po::value<double>()->default_value(90.0),
         "Maximum declination bound (deg) for the duplication region.")
        ("chunk-id", po::value<vector<int32_t> >(),
         "Optionally limit duplication to one or more chunks. If specified, "
         "data will be duplicated for the given chunk(s) regardless of the "
         "the duplication region and node.")
        ("out.node", po::value<uint32_t>(),
         "Optionally limit duplication to chunks for the given output node. "
         "A chunk is assigned to a node when the hash of the chunk ID modulo "
         "the number of nodes is equal to the node number. If this option is "
         "specified, its value must be less than --out.num-nodes. It is "
         "ignored if --chunk-id is specified.");
    po::options_description part("\\_______________ Partitioning", 80);
    part.add_options()
        ("part.index", po::value<string>(),
         "HTM index of partitioning positions. For example, if duplicating "
         "a source table partitioned on associated object RA and Dec, this "
         "would be the name of the HTM index file for the object table. If "
         "this option is omitted, then --index is used as the HTM index for "
         "both the input and partitioning position data sets.")
        ("part.prefix", po::value<string>()->default_value("chunk"),
         "Chunk file name prefix.");
    Chunker::defineOptions(part);
    opts.add(dup).add(part);
    defineOutputOptions(opts);
}


void estimateStats(ChunkIndex            & chunkIndex,
                   vector<int32_t> const & chunks,
                   Chunker         const & chunker,
                   HtmIndex        const & index,
                   HtmIndex        const & partIndex)
{
    vector<int32_t> subChunks;
    vector<uint32_t> htmIds;
    // loop over chunks
    for (vector<int32_t>::size_type i = 0; i < chunks.size(); ++i) {
        int32_t chunkId = chunks[i];
        subChunks.clear();
        chunker.getSubChunks(subChunks, chunkId);
        // loop over sub-chunks
        for (vector<int32_t>::size_type j = 0; j < subChunks.size(); ++j) {
            int32_t subChunkId = subChunks[j];
            SphericalBox box = chunker.getSubChunkBounds(chunkId, subChunkId);
            SphericalBox overlapBox = box;
            overlapBox.expand(chunker.getOverlap());
            htmIds.clear();
            box.htmIds(htmIds, index.getLevel());
            // loop over overlapping triangles
            for (vector<uint32_t>::size_type k = 0; k < htmIds.size(); ++k) {
                uint32_t targetHtmId = htmIds[k];
                uint32_t sourceHtmId = partIndex.mapToNonEmpty(targetHtmId);
                SphericalTriangle tri(targetHtmId);
                double a = tri.area();
                double x = min(tri.intersectionArea(box), a);
                ChunkLocation loc;
                loc.chunkId = chunkId;
                loc.subChunkId = subChunkId;
                loc.kind = ChunkLocation::NON_OVERLAP;
                uint64_t inTri = index(sourceHtmId);
                size_t inBox = static_cast<size_t>((x/a)*inTri);
                chunkIndex.add(loc, inBox);
                double ox = max(min(tri.intersectionArea(overlapBox), a), x);
                size_t inOverlap = static_cast<size_t>((ox/a)*inTri) - inBox;
                loc.kind = ChunkLocation::FULL_OVERLAP;
                chunkIndex.add(loc, inOverlap);
                // TODO(smm): estimate self-overlap record count?             
            }
        }
    }
}


shared_ptr<ChunkIndex> const estimateStats(po::variables_map const & vm) {
    Chunker chunker(vm);
    if (vm.count("index") == 0 && vm.count("part.index") == 0) {
        throw runtime_error("One or both of the --index and --part.index "
                            "options must be specified.");
    }
    char const * opt = (vm.count("index") != 0 ? "index" : "part.index");
    fs::path indexPath(vm[opt].as<string>());
    opt = (vm.count("part.index") != 0 ? "part.index" : "index");
    fs::path partIndexPath(vm[opt].as<string>());
    shared_ptr<HtmIndex> index(new HtmIndex(indexPath));
    shared_ptr<HtmIndex> partIndex;
    if (partIndexPath != indexPath) {
        partIndex.reset(new HtmIndex(partIndexPath));
    } else {
        partIndex = index;
    }
    if (index->getLevel() != partIndex->getLevel()) {
        throw runtime_error("Subdivision levels of input index (--index) and "
                            "partitioning index (--part.index) do not match.");
    }
    vector<int32_t> chunks = chunksToDuplicate(chunker, vm);
    shared_ptr<ChunkIndex> chunkIndex(new ChunkIndex());
    if (vm.count("verbose") != 0) {
        cerr << "Processing " << chunks.size() <<" chunks" << endl;
    }
    estimateStats(*chunkIndex, chunks, chunker, *index, *partIndex);
    return chunkIndex;
}

}}}} // namespace lsst::qserv::admin::dupr


static char const * help =
    "The Qserv duplication statistics estimator estimates the row count\n"
    "for each chunk and sub-chunk in a duplicated data-set, allowing\n"
    "partitioning parameters to be tuned without actually running the\n"
    "duplicator.\n";

int main(int argc, char const * const * argv) {
    namespace dupr = lsst::qserv::admin::dupr;
    try {
        po::options_description options;
        dupr::defineOptions(options);
        po::variables_map vm;
        dupr::parseCommandLine(vm, options, argc, argv, help);
        dupr::makeOutputDirectory(vm, true);
        shared_ptr<dupr::ChunkIndex> index = dupr::estimateStats(vm);
        if (!index->empty()) {
            fs::path d(vm["out.dir"].as<string>());
            fs::path f = vm["part.prefix"].as<string>() + "_index.bin";
            index->write(d / f, true);
        }
        if (vm.count("verbose") != 0) {
            index->write(cout, 0);
            cout << endl;
        } else {
            cout << *index << endl;
        }
    } catch (exception const & ex) {
        cerr << ex.what() << endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
