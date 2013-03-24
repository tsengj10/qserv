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
/// \brief Assignment of points on the sky to chunks and sub-chunks
///        according to the Qserv partitioning strategy.

#ifndef LSST_QSERV_ADMIN_DUPR_CHUNKER_H
#define LSST_QSERV_ADMIN_DUPR_CHUNKER_H

#include <stdint.h>
#include <utility>
#include <vector>

#include "boost/program_options.hpp"
#include "boost/scoped_array.hpp"

#include "Constants.h"
#include "Geometry.h"
#include "Hash.h"


namespace lsst { namespace qserv { namespace admin { namespace dupr {

/// Clamp `ra` to be at most 360 degrees. Any input satisfying
///
///     ra >= 360.0 - EPSILON_DEG
///
/// is mapped to 360.0. This is useful when multiplying a (sub-)chunk width
/// by an integer to obtain (sub-)chunk bounds, as this multiplication is not
/// guaranteed to give a maximum right ascension of exactly 360.0 degrees for
/// the last (sub-)chunk in a (sub-)stripe.
inline double clampRa(double ra) {
    if (ra > 360.0 - EPSILON_DEG) {
        return 360.0;
    }
    return ra;
}

/// Compute the number of segments to divide the given declination range
/// (stripe) into. Two points in the declination range separated by at least
/// one segment are guaranteed to have an angular separation of at least
/// `width`. All inputs are expected to be in units of degrees.
int segments(double decMin, double decMax, double width);

/// Return the angular width of a single segment obtained by chopping the
/// declination stripe `[decMin,decMax]` into `numSegments` equal width (in
/// right ascension) segments. Declinations must be in units of degrees.
double segmentWidth(double decMin, double decMax, int numSegments);


/// A chunk location for a position on the sky.
struct ChunkLocation {
    enum Kind {
        NON_OVERLAP = 0,
        SELF_OVERLAP,
        FULL_OVERLAP,
        NUM_KINDS
    };
    int32_t chunkId;
    int32_t subChunkId;
    Kind    kind;

    ChunkLocation() : chunkId(-1), subChunkId(-1), kind(NON_OVERLAP) { }

    /// Hash chunk locations by chunk ID.
    uint32_t hash() const { return mulveyHash(chunkId); }

    /// Order chunk locations by chunk ID.
    bool operator<(ChunkLocation const & loc) const {
        return chunkId < loc.chunkId;
    }
};


/// A Chunker locates points according to the Qserv partitioning scheme.
/// Also provided are methods for retrieving bounding boxes of chunks and
/// sub-chunks, as well as for assigning chunks to (Qserv worker) nodes.
class Chunker {
public:
    Chunker(double overlap,
            int32_t numStripes,
            int32_t numSubStripesPerStripe);

    Chunker(boost::program_options::variables_map const & vm);

    ~Chunker();

    double getOverlap() const { return _overlap; }

    /// Return a bounding box for the given chunk.
    SphericalBox const getChunkBounds(int32_t chunkId) const;

    /// Return a bounding box for the given sub-chunk.
    SphericalBox const getSubChunkBounds(int32_t chunkId,
                                         int32_t subChunkId) const;

    /// Find the non-overlap location of the given position.
    ChunkLocation const locate(
        std::pair<double, double> const & position) const;

    /// Append the locations of the given position to the `locations` vector.
    /// If `chunkId` is negative, all locations are appended. Otherwise, only
    /// those in the corresponding chunk are appended.
    void locate(std::pair<double, double> const & position,
                int32_t chunkId,
                std::vector<ChunkLocation> & locations) const;

    /// Return IDs of all chunks overlapping the given box and belonging
    /// to the given node. The target node is specified as an integer in the
    /// range `[0, numNodes)`. If `hash` is true, then the chunk with ID C is
    /// assigned to the node given by hash(C) modulo `numNodes`. Otherwise,
    /// chunks are assigned to nodes in round-robin fashion. The region
    /// argument has no effect on which node a chunk is assigned to.
    std::vector<int32_t> const getChunksFor(SphericalBox const & region,
                                            uint32_t node,
                                            uint32_t numNodes,
                                            bool hashChunks) const;

    /// Define configuration variables for partitioning.
    static void defineOptions(
        boost::program_options::options_description & opts);

private:
    // Disable copy construction and assignment.
    Chunker(Chunker const &);
    Chunker & operator=(Chunker const &);

    void _initialize(double overlap,
                     int32_t numStripes,
                     int32_t numSubStripesPerStripe);

    // Conversion between IDs and indexes.
    int32_t _getStripe(int32_t chunkId) const {
        return chunkId / (2*_numStripes);
    }
    int32_t _getSubStripe(int32_t subChunkId, int32_t stripe) const {
        return stripe*_numSubStripesPerStripe + subChunkId/_maxSubChunksPerChunk;
    }
    int32_t _getChunk(int32_t chunkId, int32_t stripe) const {
        return chunkId - stripe*2*_numStripes;
    }
    int32_t _getSubChunk(int32_t subChunkId, int32_t stripe,
                         int32_t subStripe, int32_t chunk) const {
        return subChunkId -
               (subStripe - stripe*_numSubStripesPerStripe)*_maxSubChunksPerChunk +
               chunk*_numSubChunksPerChunk[subStripe];
    }
    int32_t _getChunkId(int32_t stripe, int32_t chunk) const {
        return stripe*2*_numStripes + chunk;
    }
    int32_t _getSubChunkId(int32_t stripe, int32_t subStripe,
                           int32_t chunk, int32_t subChunk) const {
        return (subStripe - stripe*_numSubStripesPerStripe)*_maxSubChunksPerChunk +
               (subChunk - chunk*_numSubChunksPerChunk[subStripe]);
    }

    void _upDownOverlap(double ra,
                        int32_t chunkId,
                        ChunkLocation::Kind kind,
                        int32_t stripe,
                        int32_t subStripe,
                        std::vector<ChunkLocation> & locations) const;

    double _overlap;
    double _subStripeHeight;
    int32_t _numStripes;
    int32_t _numSubStripesPerStripe;
    /// The maximum number of sub-chunks per chunk across all sub-stripes.
    int32_t _maxSubChunksPerChunk;
    /// The number of chunks per stripe, indexed by stripe.
    boost::scoped_array<int32_t> _numChunksPerStripe;
    /// The number of sub-chunks per chunk, indexed by sub-stripe.
    boost::scoped_array<int32_t> _numSubChunksPerChunk;
    /// The sub-chunk width (in RA) for each sub-stripe.
    boost::scoped_array<double> _subChunkWidth;
    /// For each sub-stripe, the maximum half-width (in RA) of a circle with
    /// radius `_overlap` and center inside the sub-stripe. Guaranteed to be
    /// smaller than the sub-chunk width.
    boost::scoped_array<double> _alpha;
};

}}}} // namespace lsst::qserv::admin::dupr

#endif // LSST_QSERV_ADMIN_DUPR_CHUNKER_H
