/*
  ==============================================================================

    Tsne.h

    Exact t-SNE (van der Maaten & Hinton, 2008) to project feature vectors (e.g.
    the 512-dimensional CLAP embeddings of Freesound sounds) to 2D, so similar
    sounds appear close to each other in the sounds map. The exact version is
    O(N^2) per iteration, which is fast enough for the few hundred points of a
    search (300 points: well under a second).

  ==============================================================================
*/

#pragma once

#include <array>
#include <cmath>
#include <functional>
#include <limits>
#include <random>
#include <vector>


namespace SourceTsne
{
    struct Options
    {
        double perplexity = 30.0;        // Reduced automatically for small datasets
        int iterations = 1000;
        int exaggerationIterations = 250;
        double exaggeration = 12.0;
        double learningRate = 200.0;
        unsigned int seed = 1;
        bool normaliseInput = true;      // L2-normalise vectors first (euclidean distance then behaves like cosine distance)
        int snapshotInterval = 0;        // If > 0, onSnapshot is called every this number of iterations (e.g. to animate)
    };

    using Point = std::array<float, 2>;

    /** Projects the vectors (all with the same size) to 2D. Points are centred and scaled to fit in [-1, 1].
        shouldStop is checked every iteration (returns an empty result if it returns true). onSnapshot receives the
        (unscaled) positions every options.snapshotInterval iterations; the final scale factor applied to them is given
        by scaleOut. */
    inline std::vector<Point> project (std::vector<std::vector<float>> data, const Options& options = {},
                                       const std::function<bool()>& shouldStop = nullptr,
                                       const std::function<void (const std::vector<Point>&)>& onSnapshot = nullptr,
                                       float* scaleOut = nullptr)
    {
        const size_t n = data.size();
        std::vector<Point> result (n, Point { 0.0f, 0.0f });
        if (n <= 1)
            return result;
        if (n == 2)
            return { Point { -1.0f, 0.0f }, Point { 1.0f, 0.0f } };

        if (options.normaliseInput)
        {
            for (auto& v : data)
            {
                double norm = 0.0;
                for (auto x : v)
                    norm += (double) x * x;
                norm = std::sqrt (norm);
                if (norm > 0.0)
                    for (auto& x : v)
                        x = (float) (x / norm);
            }
        }

        // Squared distances in the input space
        std::vector<double> distances (n * n, 0.0);
        for (size_t i = 0; i < n; ++i)
            for (size_t j = i + 1; j < n; ++j)
            {
                double d = 0.0;
                const size_t dims = std::min (data[i].size(), data[j].size());
                for (size_t k = 0; k < dims; ++k)
                {
                    const double diff = (double) data[i][k] - (double) data[j][k];
                    d += diff * diff;
                }
                distances[i * n + j] = distances[j * n + i] = d;
            }

        // Conditional probabilities with a Gaussian kernel whose width gives the requested perplexity
        const double perplexity = std::max (2.0, std::min (options.perplexity, (double) (n - 1) / 3.0));
        const double targetEntropy = std::log (perplexity);
        std::vector<double> p (n * n, 0.0);
        for (size_t i = 0; i < n; ++i)
        {
            double beta = 1.0, betaMin = -std::numeric_limits<double>::infinity(), betaMax = std::numeric_limits<double>::infinity();
            // Scale beta to the distances so the search starts in the right range
            double meanDistance = 0.0;
            for (size_t j = 0; j < n; ++j)
                meanDistance += distances[i * n + j];
            meanDistance /= (double) (n - 1);
            if (meanDistance > 0.0)
                beta = 1.0 / meanDistance;
            for (int step = 0; step < 100; ++step)
            {
                double sum = 0.0, weightedDistances = 0.0;
                for (size_t j = 0; j < n; ++j)
                {
                    const double value = (i == j) ? 0.0 : std::exp (-distances[i * n + j] * beta);
                    p[i * n + j] = value;
                    sum += value;
                    weightedDistances += distances[i * n + j] * value;
                }
                if (sum <= 0.0)
                    sum = std::numeric_limits<double>::min();
                const double entropy = std::log (sum) + beta * weightedDistances / sum;
                for (size_t j = 0; j < n; ++j)
                    p[i * n + j] /= sum;
                const double difference = entropy - targetEntropy;
                if (std::abs (difference) < 1.0e-5)
                    break;
                if (difference > 0.0)
                {
                    betaMin = beta;
                    beta = std::isinf (betaMax) ? beta * 2.0 : (beta + betaMax) * 0.5;
                }
                else
                {
                    betaMax = beta;
                    beta = std::isinf (betaMin) ? beta * 0.5 : (beta + betaMin) * 0.5;
                }
            }
        }
        // Symmetric joint probabilities
        for (size_t i = 0; i < n; ++i)
            for (size_t j = i + 1; j < n; ++j)
            {
                const double value = std::max ((p[i * n + j] + p[j * n + i]) / (2.0 * (double) n), 1.0e-12);
                p[i * n + j] = p[j * n + i] = value;
            }

        // Gradient descent (with momentum and per-parameter gains, as in the reference implementation)
        std::mt19937 random (options.seed);
        std::normal_distribution<double> normal (0.0, 1.0e-4);
        std::vector<double> y (n * 2), update (n * 2, 0.0), gains (n * 2, 1.0), gradient (n * 2);
        for (auto& v : y)
            v = normal (random);
        std::vector<double> numerators (n * n);
        for (int iteration = 0; iteration < options.iterations; ++iteration)
        {
            if (shouldStop != nullptr && shouldStop())
                return {};
            const double exaggeration = iteration < options.exaggerationIterations ? options.exaggeration : 1.0;
            const double momentum = iteration < options.exaggerationIterations ? 0.5 : 0.8;

            double sumNumerators = 0.0;
            for (size_t i = 0; i < n; ++i)
                for (size_t j = i + 1; j < n; ++j)
                {
                    const double dx = y[i * 2] - y[j * 2], dy = y[i * 2 + 1] - y[j * 2 + 1];
                    const double value = 1.0 / (1.0 + dx * dx + dy * dy);
                    numerators[i * n + j] = numerators[j * n + i] = value;
                    sumNumerators += 2.0 * value;
                }
            std::fill (gradient.begin(), gradient.end(), 0.0);
            for (size_t i = 0; i < n; ++i)
                for (size_t j = 0; j < n; ++j)
                {
                    if (i == j)
                        continue;
                    const double num = numerators[i * n + j];
                    const double q = std::max (num / sumNumerators, 1.0e-12);
                    const double factor = 4.0 * (exaggeration * p[i * n + j] - q) * num;
                    gradient[i * 2] += factor * (y[i * 2] - y[j * 2]);
                    gradient[i * 2 + 1] += factor * (y[i * 2 + 1] - y[j * 2 + 1]);
                }
            for (size_t k = 0; k < y.size(); ++k)
            {
                const bool sameSign = (gradient[k] > 0.0) == (update[k] > 0.0);
                gains[k] = std::max (0.01, sameSign ? gains[k] * 0.8 : gains[k] + 0.2);
                update[k] = momentum * update[k] - options.learningRate * gains[k] * gradient[k];
                y[k] += update[k];
            }
            // Keep the points centred
            double meanX = 0.0, meanY = 0.0;
            for (size_t i = 0; i < n; ++i)
            {
                meanX += y[i * 2];
                meanY += y[i * 2 + 1];
            }
            meanX /= (double) n;
            meanY /= (double) n;
            for (size_t i = 0; i < n; ++i)
            {
                y[i * 2] -= meanX;
                y[i * 2 + 1] -= meanY;
            }
            if (onSnapshot != nullptr && options.snapshotInterval > 0 && iteration % options.snapshotInterval == 0)
            {
                std::vector<Point> snapshot (n);
                for (size_t i = 0; i < n; ++i)
                    snapshot[i] = { (float) y[i * 2], (float) y[i * 2 + 1] };
                onSnapshot (snapshot);
            }
        }

        // Scale to [-1, 1]
        double maxAbs = 0.0;
        for (auto v : y)
            maxAbs = std::max (maxAbs, std::abs (v));
        if (scaleOut != nullptr)
            *scaleOut = maxAbs > 0.0 ? (float) (1.0 / maxAbs) : 1.0f;
        for (size_t i = 0; i < n; ++i)
            result[i] = { (float) (maxAbs > 0.0 ? y[i * 2] / maxAbs : 0.0), (float) (maxAbs > 0.0 ? y[i * 2 + 1] / maxAbs : 0.0) };
        return result;
    }
}
