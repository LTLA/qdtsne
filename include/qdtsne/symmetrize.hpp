#ifndef QDTSNE_SYMMETRIZE_HPP
#define QDTSNE_SYMMETRIZE_HPP

#include "utils.hpp"
#include <vector>
#include <algorithm>

namespace qdtsne {

template<typename Index>
void symmetrize_matrix(NeighborList<Index>& x) {
    std::vector<size_t> last(x.size());
    std::vector<size_t> original(x.size());

    double total = 0;
    for (size_t i = 0; i < x.size(); ++i) {
        auto& current = x[i];
        std::sort(current.begin(), current.end()); // sorting by ID, see below.
        original[i] = current.size();

        for (auto& y : current) {
            total += y.second;
        }
    }

    for (size_t first = 0; first < x.size(); ++first) {
        auto& current = x[first];
        const Index desired = first;

        // Looping through the neighbors and searching for self in each
        // neighbor's neighbors. Assuming that the each neighbor list is sorted
        // by index up to the original size of the list (i.e., excluding newly
        // appended elements from symmetrization), this should only require a
        // single pass through the entire set of neighbors as we do not need to
        // search previously searched hits.
        for (auto& y : current) {
            auto& target = x[y.first];
            auto& curlast = last[y.first];
            auto limits = original[y.first];
            while (curlast < limits && target[curlast].first < desired) {
                ++curlast;
            }

            if (curlast < limits && target[curlast].first == desired) {
                if (desired < y.first) { 
                    // Adding the probabilities - but if desired > y.first,
                    // then this would have already been done when y.first was
                    // 'desired'. So we skip this to avoid adding it twice.
                    double combined = y.second + target[curlast].second;
                    y.second = combined;
                    target[curlast].second = combined;
                }
            } else {
                target.emplace_back(desired, y.second);
            }
        }
    }

    // Divide the result by twice the total, so that it all sums to unity.
    total *= 2;
    for (auto& current : x) {
        for (auto& y : current) {
            y.second /= total;
        }

        // Sorting to obtain increasing indices, which should be more cache
        // friendly in the edge force calculations in tsne.hpp.
        std::sort(current.begin(), current.end());
    }

    return;
}

}

#endif
