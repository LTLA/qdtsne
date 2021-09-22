#ifndef QDTSNE_INTERPOLATE_HPP
#define QDTSNE_INTEROPLATE_HPP

#include "utils.hpp"
#include "sptree.hpp"

#include <unordered_map>
#include <array>
#include <algorithm>
#include <vector>

namespace qdtsne {

namespace interpolate {

template<int ndim>
using coords = std::array<double, ndim>;

template<int ndim>
std::array<size_t, ndim> encode(const double* data, const coords<ndim>& mins, const coords<ndim>& step, int intervals) {
    std::array<size_t, ndim> current;
    size_t limit = intervals - 1;
    for (int d = 0; d < ndim; ++d, ++data) {
        current[d] = std::min(static_cast<size_t>((*data - mins[d]) / step[d]), limit); // set upper bound to catch the value at the max.
    }
    return current;
}

template<int ndim>
size_t hash(const std::array<size_t, ndim>& index, int intervals) {
    size_t counter = 0;
    for (int d = 0; d < ndim; ++d) {
        counter *= intervals + 1; // need to +1 as we can get indices == intervals when populating corners.
        counter += index[d];
    }
    return counter;
}

template<int ndim>
coords<ndim> unhash(size_t hash, const coords<ndim>& mins, const coords<ndim>& step, int intervals) {
    coords<ndim> current;
    for (int d = 0; d < ndim; ++d) {
        int d0 = ndim - d - 1;
        current[d0] = (hash % (intervals + 1)) * step[d0] + mins[d0];
        hash /= intervals + 1;
    }
    return current;
}

template<int ndim>
std::array<size_t, ndim> unhash(size_t hash, int intervals) {
    std::array<size_t, ndim> current;
    for (int d = 0; d < ndim; ++d) {
        current[ndim - d - 1] = (hash % (intervals + 1));
        hash /= intervals + 1;
    }
    return current;
}

template<int ndim, int d = 0>
double populate_corners(std::unordered_map<size_t, size_t>& collected, std::array<size_t, ndim> current, int intervals, int shifted = 0) {
    if constexpr(d == ndim - 1) {
        auto fill = [&](const std::array<size_t, ndim>& x) -> void {
            size_t h = hash<ndim>(x, intervals);
            auto it = collected.find(h);
            if (it == collected.end()) {
                collected[h] = -1;
            }
        };

        // Must have at least one hit.
        if (shifted) {
            fill(current);
            ++current[d];
            fill(current);
        } else {
            ++current[d];
            fill(current);
        }
    } else {
        populate_corners<ndim, d + 1>(collected, current, intervals, shifted);
        ++current[d];
        populate_corners<ndim, d + 1>(collected, current, intervals, shifted + 1);
    }
}

template<int ndim=2>
double compute_non_edge_forces(
    const SPTree<ndim>& tree, 
    size_t N, 
    const double* Y, 
    double theta, 
    double* neg, 
    int intervals
#ifdef _OPENMP
    , std::vector<double>& omp_buffer
#endif
) {
    // Get the limits of the existing coordinates.
    coords<ndim> mins, maxs;
    std::fill_n(mins.begin(), ndim, std::numeric_limits<double>::max());
    std::fill_n(maxs.begin(), ndim, std::numeric_limits<double>::lowest());
    {
        const auto* copy = Y;
        for (size_t i = 0; i < N; ++i) {
            for (int d = 0; d < ndim; ++d, ++copy) {
                mins[d] = std::min(mins[d], *copy);
                maxs[d] = std::max(maxs[d], *copy);
            }
        }
    }

    coords<ndim> step;
    for (int d = 0; d < ndim; ++d) {
        step[d] = (maxs[d] - mins[d]) / intervals;
        if (step[d] == 0) {
            step[d] = 1e-8; 
        }
    }

    // First pass to identify all occupied waypoints.
    std::unordered_map<size_t, size_t> waypoints;
    for (size_t i = 0; i < N; ++i) {
        auto current = encode<ndim>(Y + i * ndim, mins, step, intervals); 

        size_t counter = hash<ndim>(current, intervals);
        auto wIt = waypoints.find(counter);
        bool redo = false;
        if (wIt == waypoints.end()) {
            waypoints[counter] = 0;
            redo = true;
        } else if (wIt->second > 0) {
            wIt->second = 0;
            redo = true;
        }

        if (redo) {
            populate_corners<ndim>(waypoints, current, intervals);
        }
    }

    // Second pass to compute forces for the waypoints.
    std::unordered_map<size_t, size_t> has_zero;
    has_zero.reserve(waypoints.size());
    constexpr int nvalues = ndim + 1;
    std::vector<double> collected(nvalues * waypoints.size());
#ifdef _OPENMP
    std::vector<size_t> indices(waypoints.size());
#endif
    {
        size_t i = 0;
        for (auto wIt = waypoints.begin(); wIt != waypoints.end(); ++wIt, ++i) {
            auto h = wIt->first;
            if (wIt->second == 0) {
                has_zero[h] = has_zero.size();
            }
            wIt->second = i;
#ifdef _OPENMP
            indices[i] = h;
#else
            auto current = unhash<ndim>(h, mins, step, intervals);
            double* curcollected = collected.data() + nvalues * i;
            curcollected[ndim] = tree.compute_non_edge_forces(current.data(), theta, curcollected);
#endif
        }
    }

#ifdef _OPENMP
    #pragma omp parallel for
    for (size_t i = 0; i < indices.size(); ++i) {
        auto current = unhash<ndim>(h, mins, step, intervals);
        double* curcollected = collected.data() + nvalues * i;
        curcollected[ndim] = tree.compute_non_edge_forces(current.data(), theta, curcollected);
    }
#endif

    // Third pass to precompute the interpolating structures. 
    if constexpr(ndim != 2) {
        throw std::runtime_error("interpolation is not yet supported for ndim != 2");
    }
    constexpr int ncorners = (1 << ndim);
    size_t blocksize = ncorners * nvalues;
    std::vector<double> interpolants(blocksize * has_zero.size());

    for (const auto& y : has_zero) {
        auto current = unhash<ndim>(y.first, intervals);

        // Finding the other points in the same box, by traversing the corners.
        std::array<size_t, ncorners> others; 
        others[0] = waypoints[hash<ndim>(current, intervals)];
        ++current[0];
        others[1] = waypoints[hash<ndim>(current, intervals)];
        ++current[1];
        others[3] = waypoints[hash<ndim>(current, intervals)];
        --current[0];
        others[2] = waypoints[hash<ndim>(current, intervals)];

        // Computing the slopes and intercepts.
        for (int d = 0; d <= ndim; ++d) {
            std::array<double, ncorners> obs;
            for (size_t o = 0; o < others.size(); ++o) {
                obs[o] = collected[nvalues * others[o] + d];
            }

            double slope0 = (obs[1] - obs[0]) / step[0];
            double intercept0 = obs[0];
            double slope1 = (obs[3] - obs[2]) / step[0];
            double intercept1 = obs[2];

            size_t offset = y.second * blocksize + d * (1 << ndim);
            interpolants[offset + 0] = (slope1 - slope0) / step[1]; // slope of the slope.
            interpolants[offset + 1] = slope0; // intercept of the slope.
            interpolants[offset + 2] = (intercept1 - intercept0) / step[1]; // slope of the intercept.
            interpolants[offset + 3] = intercept0; // intercept of the intercept.
        }
    }

    // Final pass for the actual interpolation. 
#ifndef _OPENMP    
    double output_sum = 0;
#endif
    for (size_t i = 0; i < N; ++i) {
        auto copy = Y + i * ndim;
        auto current = encode<ndim>(copy, mins, step, intervals);
        std::array<double, ndim> delta;
        for (int d = 0; d < ndim; ++d, ++copy) {
            delta[d] = *copy - (current[d] * step[d] + mins[d]);
        }

        size_t h = hash<ndim>(current, intervals);
        size_t counter = has_zero[h]; // this had better not miss!
        double current_sum = 0;

        for (int d = 0; d <= ndim; ++d) {
            size_t offset = counter * blocksize + d * ncorners;
            double slope = interpolants[offset] * delta[1] + interpolants[offset + 1];
            double intercept = interpolants[offset + 2] * delta[1] + interpolants[offset + 3];
            auto& output = (d == ndim ? current_sum : neg[i * ndim + d]);
            output = slope * delta[0] + intercept;
        }

#ifdef _OPENMP
        omp_buffer[i] = current_sum;
#else
        output_sum += current_sum;
#endif
    }

#ifdef _OPENMP
    return std::accumulate(omp_buffer.begin(), omp_buffer.end(), 0.0);
#else
    return output_sum;
#endif
}

}

}

#endif