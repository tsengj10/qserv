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

#include "ChunkIndex.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <stdexcept>
#include <utility>

#include "boost/scoped_array.hpp"

#include "Constants.h"
#include "FileUtils.h"

using std::floor;
using std::nth_element;
using std::numeric_limits;
using std::ostream;
using std::pair;
using std::pow;
using std::runtime_error;
using std::setprecision;
using std::setw;
using std::sort;
using std::sqrt;
using std::string;
using std::vector;

namespace fs = boost::filesystem;


namespace lsst { namespace qserv { namespace admin { namespace dupr {

void ChunkIndex::Stats::clear() {
    n = 0;
    min = 0;
    max = 0;
    quartile[0] = 0; quartile[1] = 0; quartile[2] = 0;
    sigma = numeric_limits<double>::quiet_NaN();
    skewness = numeric_limits<double>::quiet_NaN();
    kurtosis = numeric_limits<double>::quiet_NaN();
}

namespace {
    vector<uint64_t>::iterator percentile(double p, vector<uint64_t> & v) {
        typedef vector<uint64_t>::size_type S;
        S i = std::min(static_cast<S>(floor(p*v.size() + 0.5)), v.size());
        return v.begin() + i;
    }
}

void ChunkIndex::Stats::set(vector<uint64_t> & counts) {
    typedef vector<uint64_t>::iterator Iter;
    if (counts.empty()) {
        clear();
        return;
    }
    n = counts.size();
    nrec = 0;
    max = 0;
    min = numeric_limits<uint64_t>::max();
    // Compute sum, min, and max of record counts.
    for (Iter i = counts.begin(), e = counts.end(); i != e; ++i) {
        uint64_t c = *i;
        nrec += c;
        if (c < min) { min = c; }
        if (c > max) { max = c; }
    }
    n = counts.size();
    Iter q1 = percentile(0.25, counts);
    Iter q2 = percentile(0.5,  counts);
    Iter q3 = percentile(0.75, counts);
    Iter end = counts.end();
    nth_element(counts.begin(), q1, end);
    nth_element(q1, q2, end);
    nth_element(q2, q3, end);
    quartile[0] = *q1;
    quartile[1] = *q2;
    quartile[2] = *q3;
    mean = static_cast<double>(nrec) / static_cast<double>(n);
    // Compute moments of the record count distribution.
    double m2 = 0.0, m3 = 0.0, m4 = 0.0;
    for (Iter c = counts.begin(), e = counts.end(); c != e; ++c) {
        double d = static_cast<double>(*c) - mean;
        double d2 = d*d;
        m2 += d2;
        m3 += d2*d;
        m4 += d2*d2;
    }
    m2 /= n;
    m3 /= n;
    m4 /= n;
    sigma = sqrt(m2);
    skewness = m3/pow(m2, 1.5);
    kurtosis = m4/(m2*m2) - 3.0;
}

void ChunkIndex::Stats::write(ostream & os, string const & indent) const {
    os << indent << "\"nrec\":     " << nrec << ",\n"
       << indent << "\"n\":        " << n << ",\n"
       << indent << "\"min\":      " << min << ",\n"
       << indent << "\"max\":      " << max << ",\n"
       << indent << "\"quartile\": [" << quartile[0] << ", "
                                      << quartile[1] << ", "
                                      << quartile[2] << "],\n"
       << indent << "\"mean\":     " << setprecision(2) << mean << ",\n"
       << indent << "\"sigma\":    " << setprecision(3) << sigma << ",\n"
       << indent << "\"skewness\": " << setprecision(3) << skewness << ",\n"
       << indent << "\"kurtosis\": " << setprecision(3) << kurtosis;
}


ChunkIndex::ChunkIndex() :
    _chunks(),
    _subChunks(),
    _modified(false),
    _chunkStats(),
    _subChunkStats()
{ }

ChunkIndex::ChunkIndex(fs::path const & path) :
    _chunks(),
    _subChunks(),
    _modified(false),
    _chunkStats(),
    _subChunkStats()
{
    _read(path);
}

ChunkIndex::ChunkIndex(vector<fs::path> const & paths) :
    _chunks(),
    _subChunks(),
    _modified(false),
    _chunkStats(),
    _subChunkStats()
{
    typedef vector<fs::path>::const_iterator Iter;
    for (Iter i = paths.begin(), e = paths.end(); i != e; ++i) {
        _read(*i);
    }
}

ChunkIndex::ChunkIndex(ChunkIndex const & idx) :
    _chunks(idx._chunks),
    _subChunks(idx._subChunks),
    _modified(idx._modified),
    _chunkStats(idx._chunkStats),
    _subChunkStats(idx._subChunkStats)
{ }

ChunkIndex::~ChunkIndex() { }

void ChunkIndex::write(fs::path const & path, bool truncate) const {
    size_t numBytes = _subChunks.size()*static_cast<size_t>(ENTRY_SIZE);
    boost::scoped_array<uint8_t> buf(new uint8_t[numBytes]);
    uint8_t * b = buf.get();
    for (SubChunkIter i = _subChunks.begin(), e = _subChunks.end();
         i != e; ++i) {
        Entry const & entry = i->second;
        b = encode(b, static_cast<uint64_t>(i->first));
        for (int j = 0; j < ChunkLocation::NUM_KINDS; ++j) {
            b = encode(b, entry.numRecords[j]);
        }
    }
    OutputFile f(path, truncate);
    f.append(buf.get(), numBytes);
}


namespace {
    template <typename K> struct EntryPairCmp {
        bool operator()(pair<K, ChunkIndex::Entry> const & p1,
                        pair<K, ChunkIndex::Entry> const & p2) const {
            return p1.first < p2.first;
        }
    };
}

void ChunkIndex::write(ostream & os, int verbosity) const {
    typedef pair<int32_t, Entry> Chunk;
    typedef pair<int64_t, Entry> SubChunk;

    static string const INDENT("\t\t");
    if (_modified) {
        _computeStats();
    }
    os << "{\n"
          "\"chunkStats\": [\n"
          "\t{\n";
    for (int j = 0; j < ChunkLocation::NUM_KINDS; ++j) {
        if (j > 0) {
            os << ", {\n";
        }
        _chunkStats[j].write(os, INDENT);
        os << "\n\t}";
    }
    os << "\n],\n"
          "\"subChunkStats\": [\n"
          "\t{\n";
    for (int j = 0; j < ChunkLocation::NUM_KINDS; ++j) {
        if (j > 0) {
            os << ", {\n";
        }
        _subChunkStats[j].write(os, INDENT);
        os << "\n\t}";
    }
    os << "\n]";
    if (verbosity < 0) {
        os << "\n}";
        return;
    }
    os << ",\n"
          "\"chunks\": [\n";
    // Extract and sort non-empty chunks and sub-chunks.
    vector<Chunk> chunks;
    vector<SubChunk> subChunks;
    chunks.reserve(_chunks.size());
    for (ChunkIter c = _chunks.begin(), e = _chunks.end(); c != e; ++c) {
        chunks.push_back(*c);
    }
    sort(chunks.begin(), chunks.end(), EntryPairCmp<int32_t>());
    if (verbosity > 0) {
        subChunks.reserve(_subChunks.size());
        for (SubChunkIter s = _subChunks.begin(), e = _subChunks.end();
             s != e; ++s) {
            subChunks.push_back(*s);
        }
        sort(subChunks.begin(), subChunks.end(), EntryPairCmp<int64_t>());
    }
    // Print out chunk record counts.
    size_t sc = 0;
    for (size_t c = 0; c < chunks.size(); ++c) {
        if (c > 0) {
            os << ",\n";
        }
        int32_t const chunkId = chunks[c].first;
        Entry const * e = &chunks[c].second;
        os << "\t{\"id\":  " << setw(7) << chunkId << ", \"nrec\": [";
        for (int j  = 0; j < ChunkLocation::NUM_KINDS; ++j) {
            if (j > 0) {
                os << ", ";
            }
            os << setw(8) << e->numRecords[j];
        }
        if (verbosity > 0) {
            // Print record counts for sub-chunks of chunkId.
            os << ", \"subchunks\": [\n";
            size_t s = sc;
            for (; s < subChunks.size(); ++s) {
                if ((subChunks[s].first >> 32) != chunkId) {
                    break;
                }
                if (s > sc) {
                    os << ",\n";
                }
                int32_t subChunkId = static_cast<int32_t>(
                    subChunks[s].first & 0xfffffff);
                e = &subChunks[s].second;
                os << "\t\t{\"id\":" << setw(7) << subChunkId
                   << ", \"nrec\": [";
                for (int j = 0; j < ChunkLocation::NUM_KINDS; ++j) {
                    if (j > 0) {
                        os << ", ";
                    }
                    os << setw(6) << e->numRecords[j];
                }
            }
            os << "\n\t]\n\t";
        }
        os << "]}";
    }
    os << "\n]\n}";
}

void ChunkIndex::add(ChunkLocation const & loc, size_t n) {
    if (n == 0) {
        return;
    }
    Entry * c = &_chunks[loc.chunkId];
    Entry * sc = &_subChunks[_key(loc.chunkId, loc.subChunkId)];
    c->numRecords[loc.kind] += n;
    sc->numRecords[loc.kind] += n;
    if (loc.kind == ChunkLocation::SELF_OVERLAP) {
        c->numRecords[ChunkLocation::FULL_OVERLAP] += n;
        sc->numRecords[ChunkLocation::FULL_OVERLAP] += n;
    }
    _modified = true;
}

void ChunkIndex::merge(ChunkIndex const & idx) {
    if (this == &idx || idx.empty()) {
        return;
    }
    _modified = true;
    for (ChunkIter c = idx._chunks.begin(), e = idx._chunks.end();
         c != e; ++c) {
        _chunks[c->first] += c->second;
    }
    for (SubChunkIter s = idx._subChunks.begin(), e = idx._subChunks.end();
         s != e; ++s) {
        _subChunks[s->first] += s->second;
    }
}

void ChunkIndex::clear() {
    _chunks.clear();
    _subChunks.clear();
    _modified = false;
    for (int j = 0; j < ChunkLocation::NUM_KINDS; ++j) {
        _chunkStats[j].clear();
        _subChunkStats[j].clear();
    }
}

void ChunkIndex::swap(ChunkIndex & idx) {
    using std::swap;
    if (this != &idx) {
        swap(_chunks, idx._chunks);
        swap(_subChunks, idx._subChunks);
        swap(_modified, idx._modified);
        swap(_chunkStats, idx._chunkStats);
        swap(_subChunkStats, idx._subChunkStats);
    }
}

ChunkIndex::Entry const ChunkIndex::EMPTY;

void ChunkIndex::_read(fs::path const & path) {
    InputFile f(path);
    if (f.size() % ENTRY_SIZE != 0) {
        throw runtime_error("Invalid chunk index file.");
    }
    if (f.size() == 0) {
        return;
    }
    boost::scoped_array<uint8_t> data(new uint8_t[f.size()]);
    f.read(data.get(), 0, f.size());
    uint8_t const * b = data.get();
    uint8_t const * e = b + f.size();
    _modified = true;
    while (b < e) {
        int64_t id = static_cast<int64_t>(decode<uint64_t>(b));
        b += 8;
        int32_t chunkId = static_cast<int32_t>(id >> 32);
        Entry entry;
        for (int j = 0; j < ChunkLocation::NUM_KINDS; ++j, b += 8) {
            entry.numRecords[j] = decode<uint64_t>(b);
        }
        _chunks[chunkId] += entry;
        _subChunks[id] += entry;
    }
}

void ChunkIndex::_computeStats() const {
    if (_chunks.empty()) {
        for (int j = 0; j < ChunkLocation::NUM_KINDS; ++j) {
            _chunkStats[j].clear();
            _subChunkStats[j].clear();
        }
        return;
    }
    vector<uint64_t> counts;
    counts.reserve(_subChunks.size());
    _modified = true;
    for (int j = 0; j < ChunkLocation::NUM_KINDS; ++j) {
        for (ChunkIter c = _chunks.begin(), e = _chunks.end(); c != e; ++c) {
            counts.push_back(c->second.numRecords[j]);
        }
        _chunkStats[j].set(counts);
        counts.clear();
        for (SubChunkIter s = _subChunks.begin(), e = _subChunks.end(); s != e; ++s) {
             counts.push_back(s->second.numRecords[j]);
        }
        _subChunkStats[j].set(counts);
        counts.clear();
    }
}

}}}} // namespace lsst::qserv::admin::dupr
