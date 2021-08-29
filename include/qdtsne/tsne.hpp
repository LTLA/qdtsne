/*
 *
 * Copyright (c) 2014, Laurens van der Maaten (Delft University of Technology)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by the Delft University of Technology.
 * 4. Neither the name of the Delft University of Technology nor the names of
 *    its contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY LAURENS VAN DER MAATEN ''AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL LAURENS VAN DER MAATEN BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 */

#ifndef QDTSNE_TSNE_HPP
#define QDTSNE_TSNE_HPP

#include "sptree.hpp"

#ifndef QDTSNE_CUSTOM_NEIGHBORS
#include "knncolle/knncolle.hpp"
#endif

#include <vector>
#include <cmath>
#include <algorithm>
#include <type_traits>

/**
 * @file tsne.hpp
 *
 * @brief Implements the t-SNE algorithm.
 */

namespace qdtsne {

/**
 * @brief Implements the t-SNE algorithm.
 *
 * The t-distributed stochastic neighbor embedding (t-SNE) algorithm is a non-linear dimensionality reduction technique for visualizing high-dimensional datasets.
 * It places each observation in a low-dimensional map (usually 2D) in a manner that preserves the identity of its neighbors in the original space, thus preserving the local structure of the dataset.
 * This is achieved by converting the distances between neighbors in high-dimensional space to probabilities via a Gaussian kernel;
 * creating a low-dimensional representation where the distances between neighbors can be converted to similar probabilities (in this case, with a t-distribution);
 * and then iterating such that the Kullback-Leiber divergence between the two probability distributions is minimized.
 * In practice, this involves balancing the attractive forces between neighbors and repulsive forces between all points.
 *
 * @tparam Number of dimensions of the final embedding.
 * Values typically range from 2-3. 
 *
 * @see
 * van der Maaten, L.J.P. and Hinton, G.E. (2008). 
 * Visualizing high-dimensional data using t-SNE. 
 * _Journal of Machine Learning Research_, 9, 2579-2605.
 *
 * @see 
 * van der Maaten, L.J.P. (2014). 
 * Accelerating t-SNE using tree-based algorithms. 
 * _Journal of Machine Learning Research_, 15, 3221-3245.
 */
template <int ndim=2>
class Tsne {
public:
    /**
     * @brief Default parameters for t-SNE iterations.
     */
    struct Defaults {
        /**
         * See `set_perplexity()`.
         */
        static constexpr double perplexity = 30;

        /**
         * See `set_theta()`.
         */
        static constexpr double theta = 0.5;

        /**
         * See `set_max_iter()`.
         */
        static constexpr int max_iter = 1000;

        /**
         * See `set_stop_lying_iter()`.
         */
        static constexpr int stop_lying_iter = 250;

        /**
         * See `set_mom_switch_iter()`.
         */
        static constexpr int mom_switch_iter = 250;

        /**
         * See `set_start_momentum()`.
         */
        static constexpr double start_momentum = 0.5;

        /**
         * See `set_final_momentum()`.
         */
        static constexpr double final_momentum = 0.8;

        /**
         * See `set_eta()`.
         */
        static constexpr double eta = 200;

        /**
         * See `set_exaggeration_factor()`.
         */
        static constexpr double exaggeration_factor = 12;

        /**
         * See `set_max_depth()`.
         */
        static constexpr int max_depth = 7;
    };

private:
    double perplexity = Defaults::perplexity;
    double theta = Defaults::theta;
    int max_iter = Defaults::max_iter;
    int stop_lying_iter = Defaults::stop_lying_iter;
    int mom_switch_iter = Defaults::mom_switch_iter;
    double start_momentum = Defaults::start_momentum;
    double final_momentum = Defaults::final_momentum;
    double eta = Defaults::eta;
    double exaggeration_factor = Defaults::exaggeration_factor;
    int max_depth = Defaults::max_depth;

public:
    /**
     * @param m Maximum number of iterations to perform.
     *
     * @return A reference to this `Tsne` object.
     */
    Tsne& set_max_iter(int m = Defaults::max_iter) {
        max_iter = m;
        return *this;
    }

    /**
     * @param m Maximum depth of the Barnes-Hut tree.
     * Larger values improve the quality of the approximation for the repulsive force calculation, at the cost of computational time.
     *
     * @return A reference to this `Tsne` object.
     */
    Tsne& set_max_depth(int m = Defaults::max_depth) {
        max_depth = m;
        return *this;
    }

    /**
     * @param m Number of iterations to perform before switching from the starting momentum to the final momentum.
     *
     * @return A reference to this `Tsne` object.
     *
     * The idea here is that the update to each point includes a small step in the direction of its previous update, i.e., there is some "momentum" from the previous update.
     * This aims to speed up the optimization and to avoid local minima by effectively smoothing the updates.
     * The starting momentum is usually smaller than the final momentum,
     * to give a chance for the points to improve their organization before encouraging iteration to a specific local minima.
     */
    Tsne& set_mom_switch_iter(int m = Defaults::mom_switch_iter) {
        mom_switch_iter = m;
        return *this;
    }

    /**
     * @param s Starting momentum, to be used in the early iterations before the momentum switch.
     *
     * @return A reference to this `Tsne` object.
     */
    Tsne& set_start_momentum(double s = Defaults::start_momentum) {
        start_momentum = s;
        return *this;
    }

    /**
     * @param f Final momentum, to be used in the later iterations after the momentum switch.
     *
     * @return A reference to this `Tsne` object.
     */
    Tsne& set_final_momentum(double f = Defaults::final_momentum) {
        final_momentum = f;
        return *this;
    }

    /**
     * @param s Number of iterations to perform with exaggerated probabilities, as part of the early exaggeration phase.
     *
     * @return A reference to this `Tsne` object.
     *
     * In the early exaggeration phase, the probabilities are multiplied by `m`.
     * This forces the algorithm to minimize the distances between neighbors, creating an embedding containing tight, well-separated clusters of neighboring cells.
     * Because there is so much empty space, these clusters have an opportunity to move around to find better global positions before the phase ends and they are forced to settle down.
     */
    Tsne& set_stop_lying_iter(int s = Defaults::stop_lying_iter) {
        stop_lying_iter = s;
        return *this;
    }

    /** 
     * @param e The learning rate, used to scale the updates.
     * Larger values yield larger updates that speed up convergence to a local minima at the cost of stability.
     *
     * @return A reference to this `Tsne` object.
     */
    Tsne& set_eta(double e = Defaults::eta) {
        eta = e;
        return *this;
    }

    /** 
     * @param e Factor to scale the probabilities during the early exaggeration phase.
     *
     * @return A reference to this `Tsne` object.
     */
    Tsne& set_exaggeration_factor(double e = Defaults::eta) {
        exaggeration_factor = e;
        return *this;
    }

    /**
     * @param p Perplexity, which determines the balance between local and global structure.
     * Higher perplexities will focus on global structure, at the cost of increased runtime and decreased local resolution.
     *
     * @return A reference to this `Tsne` object.
     */
    Tsne& set_perplexity(double p = Defaults::perplexity) {
        perplexity = p;
        return *this;
    }

    /** 
     * @param t Level of the approximation to use in the Barnes-Hut tree calculation of repulsive forces.
     * Lower values increase accuracy at the cost of computational time.
     *
     * @return A reference to this `Tsne` object.
     */
    Tsne& set_theta(double t = Defaults::theta) {
        theta = t;
        return *this;
    }

public:
    /**
     * @brief Current status of the t-SNE iterations.
     *
     * @tparam Index Integer type for the neighbor indices.
     *
     * This class holds the precomputed structures required to perform the t-SNE iterations.
     * Users should refrain from interacting with the internals and should only be passing it to the `Tsne::run()` method.
     */
    template<typename Index> 
    struct Status {
        /**
         * @cond
         */
        Status(size_t N, int maxdepth) : dY(N * ndim), uY(N * ndim), gains(N * ndim, 1.0), pos_f(N * ndim), neg_f(N * ndim), tree(N, maxdepth)
#ifdef _OPENMP
            , omp_buffer(N)
#endif
        {
            neighbors.reserve(N);
            probabilities.reserve(N);
            return;
        }

        std::vector<std::vector<Index> > neighbors;
        std::vector<std::vector<double> > probabilities;
        std::vector<double> dY, uY, gains, pos_f, neg_f;

#ifdef _OPENMP
        std::vector<double> omp_buffer;
#endif

        SPTree<ndim> tree;

        int iter = 0;
        /**
         * @endcond
         */

        /**
         * @return The number of iterations performed on this object so far.
         */
        int iteration() const {
            return iter;
        }
    };

public:
    /**
     * @param nn_index Vector of pointers to arrays of length `K`.
     * Each array corresponds to an observation and contains the indices to its `K` closest neighbors.
     * @param nn_dist Vector of pointers to arrays of length `K`.
     * Each array corresponds to an observation and contains the distances to its `K` closest neighbors.
     * This should be of the same length as `nn_index`.
     * @param K Number of nearest neighbors.
     *
     * @tparam Index Integer type for the neighbor indices.
     * @tparam Dist Floating-point type for the neighbor distances.
     *
     * @return A `Status` object containing various pre-computed structures required for the iterations in `run()`.
     *
     * In this initialization mode, the perplexity from `set_perplexity()` is ignored.
     * Instead, it is set to `K/3`.
     */
    template<typename Index = int, typename Dist = double>
    auto initialize(const std::vector<Index*>& nn_index, const std::vector<Dist*>& nn_dist, int K) {
        if (nn_index.size() != nn_dist.size()) {
            throw std::runtime_error("indices and distances should be of the same length");
        }

        Status<typename std::remove_const<Index>::type> status(nn_index.size(), max_depth);
        compute_gaussian_perplexity(nn_dist, K, status);
        symmetrize_matrix(nn_index, K, status);
        return status;
    }

private:
    template<typename Index, typename Dist>
    void compute_gaussian_perplexity(const std::vector<Dist*>& nn_dist, int K, Status<Index>& status) {
        const size_t N = nn_dist.size();
        constexpr double max_value = std::numeric_limits<double>::max();
        constexpr double tol = 1e-5;
        const double log_perplexity = std::log(static_cast<double>(K)/3.0); // implicitly taken from choice of 'k'.

        std::vector<std::vector<double> >& val_P = status.probabilities;
#ifdef _OPENMP
        val_P.resize(N, std::vector<double>(K));
#endif

        #pragma omp parallel
        {
            std::vector<double> squared_delta_dist(K);
            std::vector<double> quad_delta_dist(K);

            #pragma omp for 
            for (size_t n = 0; n < N; ++n){
                double beta = 1.0;
                double min_beta = 0, max_beta = max_value;
                double sum_P = 0;
                const double* distances = nn_dist[n];

#ifdef _OPENMP
                auto& output = val_P[n];
#else
                val_P.emplace_back(K);
                auto& output = val_P.back();
#endif

                // We adjust the probabilities by subtracting the first squared
                // distance from everything. This avoids problems with underflow
                // when converting distances to probabilities; it otherwise has no
                // effect on the entropy or even the final probabilities because it
                // just scales all probabilities up/down (and they need to be
                // normalized anyway, so any scaling effect just cancels out).
                const double first = distances[0] * distances[0];
                for (int m = 1; m < K; ++m) {
                    squared_delta_dist[m] = distances[m] * distances[m] - first;
                    quad_delta_dist[m] = squared_delta_dist[m] * squared_delta_dist[m];
                }
                output[0] = 1;  

                for (int iter = 0; iter < 200; ++iter) {
                    // Apply gaussian kernel. We skip the first value because
                    // we effectively normalized it to 1 by subtracting 'first'. 
                    for (int m = 1; m < K; ++m) {
                        output[m] = std::exp(-beta * squared_delta_dist[m]); 
                    }

                    sum_P = std::accumulate(output.begin() + 1, output.end(), 1.0);
                    const double prod = std::inner_product(squared_delta_dist.begin() + 1, squared_delta_dist.end(), output.begin() + 1, 0.0);
                    const double entropy = beta * (prod / sum_P) + std::log(sum_P);

                    const double diff = entropy - log_perplexity;
                    if (std::abs(diff) < tol) {
                        break;
                    }

                    // Attempt a Newton-Raphson search first. 
                    bool nr_ok = false;
#ifndef QDTSNE_BETA_BINARY_SEARCH_ONLY
                    const double prod2 = std::inner_product(quad_delta_dist.begin() + 1, quad_delta_dist.end(), output.begin() + 1, 0.0);
                    const double d1 = - beta / sum_P * (prod2 - prod * prod / sum_P);
                    if (d1) {
                        const double alt_beta = beta - (diff / d1); // if it overflows, we should get Inf or -Inf, so the following comparison should be fine.
                        if (alt_beta > min_beta && alt_beta < max_beta) {
                            beta = alt_beta;
                            nr_ok = true;
                        }
                    }
#endif

                    // Otherwise do a binary search.
                    if (!nr_ok) {
                        if (diff > 0) {
                            min_beta = beta;
                            if (max_beta == max_value) {
                                beta *= 2.0;
                            } else {
                                beta = (beta + max_beta) / 2.0;
                            }
                        } else {
                            max_beta = beta;
                            beta = (beta + min_beta) / 2.0;
                        }
                    }
                }

                // Row-normalize current row of P.
                for (auto& o : output) {
                    o /= sum_P;
                }
            }
        }

        return;
    }

private:
    template<typename Index>
    void symmetrize_matrix(const std::vector<Index*>& nn_index, int K, Status<typename std::remove_const<Index>::type>& status) {
        auto& col_P = status.neighbors;
        auto& probabilities = status.probabilities;
        const size_t N = nn_index.size();

        // Initializing the output neighbor list.
        for (size_t n = 0; n < N; ++n) {
            col_P.emplace_back(nn_index[n], nn_index[n] + K);
        }

        for (size_t n = 0; n < N; ++n) {
            auto my_neighbors = nn_index[n];

            for (int k1 = 0; k1 < K; ++k1) {
                auto curneighbor = my_neighbors[k1];
                auto neighbors_neighbors = nn_index[curneighbor];

                // Check whether the current point is present in its neighbor's set.
                bool present = false;
                for (int k2 = 0; k2 < K; ++k2) {
                    if (neighbors_neighbors[k2] == n) {
                        if (n < curneighbor) {
                            // Adding the probabilities - but if n >= curneighbor, then this would have
                            // already been done at n = curneighbor, so we skip this to avoid adding it twice.
                            double sum = probabilities[n][k1] + probabilities[curneighbor][k2];
                            probabilities[n][k1] = sum;
                            probabilities[curneighbor][k2] = sum;
                        }
                        present = true;
                        break;
                    }
                }

                if (!present) {
                    // If not present, no addition of probabilities is involved.
                    col_P[curneighbor].push_back(n);
                    probabilities[curneighbor].push_back(probabilities[n][k1]);
                }
            }
        }

        // Divide the result by two
        double total = 0;
        for (auto& x : probabilities) {
            for (auto& y : x) {
                y /= 2;
                total += y;
            }
        }

        // Probabilities across the entire matrix sum to unity.
        for (auto& x : probabilities) {
            for (auto& y : x) {
                y /= total;
            }
        }

        return;
    }

public:
    /**
     * @param nn_index Vector of pointers to arrays of length `K`.
     * Each array corresponds to an observation and contains the indices to its `K` closest neighbors.
     * @param nn_dist Vector of pointers to arrays of length `K`.
     * Each array corresponds to an observation and contains the distances to its `K` closest neighbors.
     * This should be of the same length as `nn_index`.
     * @param K Number of nearest neighbors.
     * @param[in, out] Y Pointer to a 2D array with number of rows and columns equal to `ndim` and `nn_index.size()`, respectively.
     * The array is treated as column-major where each column corresponds to an observation.
     * On input, this should contain the initial locations of each observation; on output, it is updated to the final t-SNE locations.
     *
     * @tparam Index Integer type for the neighbor indices.
     * @tparam Dist Floating-point type for the neighbor distances.
     *
     * @return A `Status` object containing the final state of the algorithm after all requested iterations are finished.
     *
     * In this initialization mode, the perplexity from `set_perplexity()` is ignored.
     * Instead, it is set to `K/3`.
     */
    template<typename Index = int, typename Dist = double>
    auto run(const std::vector<Index*>& nn_index, const std::vector<Dist*>& nn_dist, int K, double* Y) {
        auto status = initialize(nn_index, nn_dist, K);
        run(status, Y);
        return status;
    }

    /**
     * @param status The current status of the algorithm, generated either from `initialize()` or from a previous `run()` call.
     * @param[in, out] Y Pointer to a 2D array with number of rows and columns equal to `ndim` and `nn_index.size()`, respectively.
     * The array is treated as column-major where each column corresponds to an observation.
     * On input, this should contain the initial locations of each observation; on output, it is updated to the final t-SNE locations.
     *
     * @tparam Index Integer type for the neighbor indices.
     *
     * @return A `Status` object containing the final state of the algorithm after applying iterations.
     */
    template<typename Index = int>
    void run(Status<Index>& status, double* Y) {
        int& iter = status.iter;
        double multiplier = (iter < stop_lying_iter ? exaggeration_factor : 1);
        double momentum = (iter < mom_switch_iter ? start_momentum : final_momentum);

        for(; iter < max_iter; ++iter) {
            // Stop lying about the P-values after a while, and switch momentum
            if (iter == stop_lying_iter) {
                multiplier = 1;
            }
            if (iter == mom_switch_iter) {
                momentum = final_momentum;
            }

            iterate(status, Y, multiplier, momentum);
        }

        return;
    }

private:
    static double sign(double x) { 
        return (x == .0 ? .0 : (x < .0 ? -1.0 : 1.0));
    }

    template<typename Index>
    void iterate(Status<Index>& status,  double* Y, double multiplier, double momentum) {
        compute_gradient(status, Y, multiplier);

        auto& gains = status.gains;
        auto& dY = status.dY;
        auto& uY = status.uY;
        auto& col_P = status.neighbors;

        // Update gains
        for (size_t i = 0; i < gains.size(); ++i) {
            double& g = gains[i];
            g = std::max(0.01, sign(dY[i]) != sign(uY[i]) ? (g + 0.2) : (g * 0.8));
        }

        // Perform gradient update (with momentum and gains)
        for (size_t i = 0; i < gains.size(); ++i) {
            uY[i] = momentum * uY[i] - eta * gains[i] * dY[i];
            Y[i] += uY[i];
        }

        // Make solution zero-mean
        for (int d = 0; d < ndim; ++d) {
            auto start = Y + d;
            size_t N = col_P.size();

            // Compute means from column-major coordinates.
            double sum = 0;
            for (size_t i = 0; i < N; ++i, start += ndim) {
                sum += *start;
            }
            sum /= N;

            start = Y + d;
            for (size_t i = 0; i < N; ++i, start += ndim) {
                *start -= sum;
            }
        }

        return;
    }

private:
    template<typename Index>
    void compute_gradient(Status<Index>& status, const double* Y, double multiplier) {
        auto& tree = status.tree;
        tree.set(Y);

        compute_edge_forces(status, Y, multiplier);

        size_t N = status.neighbors.size();
        auto& neg_f = status.neg_f;
        std::fill(neg_f.begin(), neg_f.end(), 0);

#ifdef _OPENMP
        // Don't use reduction methods to ensure that we sum in a consistent order.
        #pragma omp parallel for
        for (size_t n = 0; n < N; ++n) {
            status.omp_buffer[n] = status.tree.compute_non_edge_forces(n, theta, neg_f.data() + n * ndim);
        }
        double sum_Q = std::accumulate(status.omp_buffer.begin(), status.omp_buffer.end(), 0.0);
#else
        double sum_Q = 0;
        for (size_t n = 0; n < N; ++n) {
            sum_Q += status.tree.compute_non_edge_forces(n, theta, neg_f.data() + n * ndim);
        }
#endif

        // Compute final t-SNE gradient
        for (size_t i = 0; i < N * ndim; ++i) {
            status.dY[i] = status.pos_f[i] - (neg_f[i] / sum_Q);
        }
    }

    template<typename Index>
    void compute_edge_forces(Status<Index>& status, const double* Y, double multiplier) {
        const auto& col_P = status.neighbors;
        const auto& val_P = status.probabilities;
        auto& pos_f = status.pos_f;
        std::fill(pos_f.begin(), pos_f.end(), 0);

        #pragma omp parallel for 
        for (size_t n = 0; n < col_P.size(); ++n) {
            const auto& cur_prob = val_P[n];
            const auto& cur_col = col_P[n];
            const double* self = Y + n * ndim;
            double* pos_out = pos_f.data() + n * ndim;

            for (size_t i = 0; i < cur_col.size(); ++i) {
                double sqdist = 0; 
                const double* neighbor = Y + cur_col[i] * ndim;
                for (int d = 0; d < ndim; ++d) {
                    sqdist += (self[d] - neighbor[d]) * (self[d] - neighbor[d]);
                }

                const double mult = multiplier * cur_prob[i] / (1 + sqdist);
                for (int d = 0; d < ndim; ++d) {
                    pos_out[d] += mult * (self[d] - neighbor[d]);
                }
            }
        }

        return;
    }

public:
#ifndef QDTSNE_CUSTOM_NEIGHBORS
    /**
     * @tparam Input Floating point type for the input data.
     * 
     * @param[in] input Pointer to a 2D array containing the input high-dimensional data, with number of rows and columns equal to `D` and `N`, respectively.
     * The array is treated as column-major where each row corresponds to a dimension and each column corresponds to an observation.
     * @param D Number of dimensions.
     * @param N Number of observations.
     *
     * @return A `Status` object containing various pre-computed structures required for the iterations in `run()`.
     *
     * This differs from the other `run()` methods in that it will internally compute the nearest neighbors for each observation.
     * As with the original t-SNE implementation, it will use vantage point trees for the search.
     * See the other `initialize()` methods to specify a custom search algorithm.
     */
    template<typename Input = double>
    auto initialize(const Input* input, size_t D, size_t N) { 
        knncolle::VpTreeEuclidean<> searcher(D, N, input); 
        return initialize(&searcher);
    }

    /**
     * @tparam Input Floating point type for the input data.
     * 
     * @param[in] input Pointer to a 2D array containing the input high-dimensional data, with number of rows and columns equal to `D` and `N`, respectively.
     * The array is treated as column-major where each row corresponds to a dimension and each column corresponds to an observation.
     * @param D Number of dimensions.
     * @param N Number of observations.
     * @param[in, out] Y Pointer to a 2D array with number of rows and columns equal to `ndim` and `nn_index.size()`, respectively.
     * The array is treated as column-major where each column corresponds to an observation.
     * On input, this should contain the initial locations of each observation; on output, it is updated to the final t-SNE locations.
     *
     * @return A `Status` object containing the final state of the algorithm after applying all iterations.
     */
    template<typename Input = double>
    auto run(const Input* input, size_t D, size_t N, double* Y) {
        auto status = initialize(input, D, N);
        run(status, Y);
        return status;
    }
#endif

    /**
     * @tparam Algorithm `knncolle::Base` subclass implementing a nearest neighbor search algorithm.
     * 
     * @param searcher Pointer to a `knncolle::Base` subclass with a `find_nearest_neighbors()` method.
     *
     * @return A `Status` object containing various pre-computed structures required for the iterations in `run()`.
     *
     * Compared to other `initialize()` methods, this provides more fine-tuned control over the nearest neighbor search parameters.
     */
    template<class Algorithm>
    auto initialize(const Algorithm* searcher) { 
        const int K = std::ceil(perplexity * 3);
        size_t N = searcher->nobs();
        if (K >= N) {
            throw std::runtime_error("number of observations should be greater than 3 * perplexity");
        }

        std::vector<int> indices(N * K);
        std::vector<double> distances(N * K);
        std::vector<const int*> nn_index(N);
        std::vector<const double*> nn_dist(N);

        #pragma omp parallel for
        for (size_t i = 0; i < N; ++i) {
            auto out = searcher->find_nearest_neighbors(i, K);
            for (size_t k = 0; k < out.size(); ++k) {
                indices[k + i *K] = out[k].first;
                distances[k + i *K] = out[k].second;
            }
            nn_index[i] = indices.data() + i * K;
            nn_dist[i] = distances.data() + i * K;
        }

        return initialize(nn_index, nn_dist, K);
    }

    /**
     * @tparam Algorithm `knncolle::Base` subclass implementing a nearest neighbor search algorithm.
     * 
     * @param searcher Pointer to a `knncolle::Base` subclass with a `find_nearest_neighbors()` method.
     * @param[in, out] Y Pointer to a 2D array with number of rows and columns equal to `ndim` and `nn_index.size()`, respectively.
     * The array is treated as column-major where each column corresponds to an observation.
     * On input, this should contain the initial locations of each observation; on output, it is updated to the final t-SNE locations.
     *
     * @return A `Status` object containing the final state of the algorithm after applying all iterations.
     */
    template<class Algorithm> 
    auto run(const Algorithm* searcher, double* Y) {
        auto status = initialize(searcher);
        run(status, Y);
        return status;
    }
};

}

#endif
