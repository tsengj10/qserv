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
/// \brief The Qserv partitioner for tables which have a single
///        partitioning position.

#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "boost/filesystem.hpp"
#include "boost/make_shared.hpp"
#include "boost/program_options.hpp"
#include "boost/shared_ptr.hpp"
#include "boost/timer/timer.hpp"

#include "Chunker.h"
#include "ChunkIndex.h"
#include "CmdLineUtils.h"
#include "Csv.h"
#include "FileUtils.h"
#include "MapReduce.h"

using std::cerr;
using std::cout;
using std::endl;
using std::exception;
using std::find;
using std::pair;
using std::runtime_error;
using std::snprintf;
using std::string;
using std::vector;

using boost::make_shared;
using boost::shared_ptr;
using boost::timer::cpu_timer;

namespace fs = boost::filesystem;
namespace po = boost::program_options;


namespace lsst { namespace qserv { namespace admin { namespace dupr {

/// Map-reduce worker class for partitioning.
///
/// The `map` function computes all partitioning locations of each input
/// record, and stores an output record per-location.
///
/// The `reduce` function saves output records to files, each containing
/// data for a single chunk ID. Chunk IDs are assigned to down-stream nodes
/// by hashing, and the corresponding output files are created in node
/// specific sub-directories of the output directory.
///
/// A worker's result is a ChunkIndex object that contains the total
/// record count for each chunk and sub-chunk seen by that worker.
class Worker : public WorkerBase<ChunkLocation, ChunkIndex> {
public:
    Worker(po::variables_map const & vm);

    void map(char const * beg, char const * end, Silo & silo);
    void reduce(RecordIter beg, RecordIter end);
    void finish();

    shared_ptr<ChunkIndex> const result() { return _index; }

    static void defineOptions(po::options_description & opts);

private:
    void _makeFilePaths(int32_t chunkId);

    csv::Editor            _editor;
    int                    _raField;
    int                    _decField;
    int                    _chunkIdField;
    int                    _subChunkIdField;
    Chunker                _chunker;
    vector<ChunkLocation>  _locations;
    shared_ptr<ChunkIndex> _index;
    int32_t                _chunkId;
    uint32_t               _numNodes;
    fs::path               _outputDir;
    fs::path               _nonOverlapPath;
    fs::path               _selfOverlapPath;
    fs::path               _fullOverlapPath;
    BufferedAppender       _nonOverlap;
    BufferedAppender       _selfOverlap;
    BufferedAppender       _fullOverlap;
};

Worker::Worker(po::variables_map const & vm) :
    _editor(vm),
    _raField(-1),
    _decField(-1),
    _chunkIdField(-1),
    _subChunkIdField(-1),
    _chunker(vm),
    _index(make_shared<ChunkIndex>()),
    _chunkId(-1),
    _numNodes(vm["out.num-nodes"].as<uint32_t>()),
    _outputDir(vm["out.dir"].as<string>()),
    _nonOverlap(vm["mr.block-size"].as<size_t>()*MiB),
    _selfOverlap(vm["mr.block-size"].as<size_t>()*MiB),
    _fullOverlap(vm["mr.block-size"].as<size_t>()*MiB)
{
    if (_numNodes == 0 || _numNodes > 99999u) {
        throw runtime_error("The --out.num-nodes option value must be "
                            "between 1 and 99999.");
    }
    // Map field names of interest to field indexes.
    if (vm.count("part.pos") == 0) {
        throw runtime_error("The --part.pos option was not specified.");
    }
    string s = vm["part.pos"].as<string>();
    pair<string,string> p = parseFieldNamePair("part.pos", s);
    _raField = _editor.getFieldIndex(p.first);
    _decField = _editor.getFieldIndex(p.second);
    if (_raField < 0 || _decField < 0) {
        throw runtime_error("--part.pos=\"" + s + "\" is not a valid "
                            "pair of input field names.");
    }
    if (vm.count("part.chunk") != 0) {
        _chunkIdField = _editor.getFieldIndex(vm["part.chunk"].as<string>());
    }
    _subChunkIdField = _editor.getFieldIndex(vm["part.sub-chunk"].as<string>());
}

void Worker::map(char const * beg, char const * end, Worker::Silo & silo) {
    typedef vector<ChunkLocation>::const_iterator LocIter;
    pair<double, double> sc;
    while (beg < end) {
        beg = _editor.readRecord(beg, end);
        sc.first = _editor.get<double>(_raField);
        sc.second = _editor.get<double>(_decField);
        // Locate partitioning position and output a record for each location.
        _locations.clear();
        _chunker.locate(sc, -1, _locations);
        assert(!_locations.empty());
        for (LocIter i = _locations.begin(), e = _locations.end(); i != e; ++i) {
            _editor.set(_chunkIdField, i->chunkId);
            _editor.set(_subChunkIdField, i->subChunkId);
            silo.add(*i, _editor);
        }
    }
}

void Worker::reduce(Worker::RecordIter beg, Worker::RecordIter end) {
    if (beg == end) {
        return;
    }
    int32_t const chunkId = beg->key.chunkId;
    if (chunkId != _chunkId) {
        finish();
        _chunkId = chunkId;
        _makeFilePaths(chunkId);
    }
    // Store records and update statistics. Files are only created/
    // opened if there is data to write to them.
    for (; beg != end; ++beg) {
        _index->add(beg->key);
        if (beg->key.kind == ChunkLocation::NON_OVERLAP) {
            if (!_nonOverlap.isOpen()) {
                _nonOverlap.open(_nonOverlapPath, false);
            }
            _nonOverlap.append(beg->data, beg->size);
        } else {
            if (beg->key.kind == ChunkLocation::SELF_OVERLAP) {
               if (!_selfOverlap.isOpen()) {
                   _selfOverlap.open(_selfOverlapPath, false);
               }
               _selfOverlap.append(beg->data, beg->size);
            }
            // self-overlap locations are also full-overlap locations.
            if (!_fullOverlap.isOpen()) {
                _fullOverlap.open(_fullOverlapPath, false);
            }
            _fullOverlap.append(beg->data, beg->size);
        }
    }
}

void Worker::finish() {
    // Reset current chunk ID to an invalid ID and close all output files.
    _chunkId = -1;
    _nonOverlap.close();
    _selfOverlap.close();
    _fullOverlap.close();
}

void Worker::defineOptions(po::options_description & opts) {
    po::options_description part("\\_______________ Partitioning", 80);
    part.add_options()
        ("incremental", po::bool_switch(),
         "Allow incrementally adding to a partitioned data set.")
        ("part.chunk", po::value<string>(),
         "Optional chunk ID output field name. This field name is appended "
         "to the output field name list if it isn't already included.")
        ("part.sub-chunk", po::value<string>()->default_value("subChunkId"),
         "Sub-chunk ID output field name. This field field name is appended "
         "to the output field name list if it isn't already included.")
        ("part.pos", po::value<string>(),
         "The partitioning right ascension and declination field names, "
         "separated by a comma.");
    Chunker::defineOptions(part);
    opts.add(part);
    defineOutputOptions(opts);
    csv::Editor::defineOptions(opts);
}

void Worker::_makeFilePaths(int32_t chunkId) {
    fs::path p = _outputDir;
    if (_numNodes > 1) {
        // Files go into a node-specific sub-directory.
        char subdir[32];
        uint32_t node = mulveyHash(static_cast<uint32_t>(chunkId)) % _numNodes;
        snprintf(subdir, sizeof(subdir), "node_%05lu",
                 static_cast<unsigned long>(node));
        p /= subdir;
        fs::create_directory(p);
    }
    char file[32];
    snprintf(file, sizeof(file), "chunk_%ld.txt",
             static_cast<long>(chunkId));
    _nonOverlapPath = p / fs::path(file);
    snprintf(file, sizeof(file), "chunk_%ld_self.txt",
             static_cast<long>(chunkId));
    _selfOverlapPath = p / fs::path(file);
    snprintf(file, sizeof(file), "chunk_%ld_full.txt",
             static_cast<long>(chunkId));
    _fullOverlapPath = p / fs::path(file);
}


typedef Job<Worker> PartitionJob;

}}}} // namespace lsst::qserv::admin::dupr


static char const * help =
    "The Qserv partitioner partitions one or more input CSV files in\n"
    "preparation for loading by Qserv worker nodes. This boils down to\n"
    "assigning each input position to locations in a 2-level subdivision\n"
    "scheme, where a location consists of a chunk and sub-chunk ID, and\n"
    "then bucket-sorting input records into output files by chunk ID.\n"
    "Chunk files can then be distributed to Qserv worker nodes for loading.\n"
    "\n"
    "A partitioned data-set can be built-up incrementally by running the\n"
    "partitioner with disjoint input file sets and the same output directory.\n"
    "Beware - the output CSV format, partitioning parameters, and worker\n"
    "node count MUST be identical between runs. Additionally, only one\n"
    "partitioner process should write to a given output directory at a\n"
    "time. If any of these conditions are not met, then the resulting\n"
    "chunk files will be corrupt and/or useless.\n";

int main(int argc, char const * const * argv) {
    namespace dupr = lsst::qserv::admin::dupr;

    try {
        cpu_timer t;
        po::options_description options;
        dupr::PartitionJob::defineOptions(options);
        po::variables_map vm;
        dupr::parseCommandLine(vm, options, argc, argv, help);
        dupr::ensureOutputFieldExists(vm, "part.chunk");
        dupr::ensureOutputFieldExists(vm, "part.sub-chunk");
        dupr::makeOutputDirectory(vm);
        dupr::PartitionJob job(vm);
        shared_ptr<dupr::ChunkIndex> index = job.run();
        if (!index->empty()) {
            fs::path d(vm["out.dir"].as<string>());
            index->write(d / "chunk_index.bin", false);
        }
        if (vm.count("verbose") != 0) {
            cerr << "run-time: " << t.format() << endl;
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
