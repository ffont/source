/*
  ==============================================================================

    SearchTests.cpp - tests for t-SNE and the Freesound search helpers (no network)

  ==============================================================================
*/

#include <JuceHeader.h>
#include "Tsne.h"
#include "FreesoundSearch.h"


class TsneTests : public juce::UnitTest
{
public:
    TsneTests() : juce::UnitTest ("t-SNE", "Source") {}

    void runTest() override
    {
        beginTest ("Small inputs");
        expect (SourceTsne::project ({}).empty());
        expectEquals ((int) SourceTsne::project ({ { 1.0f, 2.0f } }).size(), 1);
        expectEquals ((int) SourceTsne::project ({ { 1.0f, 0.0f }, { 0.0f, 1.0f } }).size(), 2);

        beginTest ("Clusters in high dimensions stay separated in 2D");
        {
            // Three clusters of 40 points around three random directions in 64 dimensions
            std::mt19937 random (7);
            std::normal_distribution<float> normal (0.0f, 1.0f);
            const int dims = 64, perCluster = 40, numClusters = 3;
            std::vector<std::vector<float>> centres (numClusters, std::vector<float> (dims));
            for (auto& c : centres)
                for (auto& v : c)
                    v = normal (random);
            std::vector<std::vector<float>> data;
            std::vector<int> labels;
            for (int c = 0; c < numClusters; ++c)
                for (int i = 0; i < perCluster; ++i)
                {
                    std::vector<float> v (dims);
                    for (int k = 0; k < dims; ++k)
                        v[(size_t) k] = centres[(size_t) c][(size_t) k] + 0.15f * normal (random);
                    data.push_back (v);
                    labels.push_back (c);
                }

            const auto start = juce::Time::getMillisecondCounterHiRes();
            const auto points = SourceTsne::project (data);
            logMessage ("t-SNE of " + juce::String ((int) data.size()) + " points: " + juce::String (juce::Time::getMillisecondCounterHiRes() - start, 0) + " ms");
            expectEquals ((int) points.size(), (int) data.size());

            // Every point is closer to its own cluster's centroid than to the others'
            std::vector<std::array<double, 2>> centroids (numClusters, { 0.0, 0.0 });
            for (size_t i = 0; i < points.size(); ++i)
            {
                centroids[(size_t) labels[i]][0] += points[i][0] / perCluster;
                centroids[(size_t) labels[i]][1] += points[i][1] / perCluster;
                expect (std::abs (points[i][0]) <= 1.0001f && std::abs (points[i][1]) <= 1.0001f, "Points are scaled to [-1, 1]");
                expect (std::isfinite (points[i][0]) && std::isfinite (points[i][1]));
            }
            int misplaced = 0;
            for (size_t i = 0; i < points.size(); ++i)
            {
                int closest = 0;
                double closestDistance = 1.0e9;
                for (int c = 0; c < numClusters; ++c)
                {
                    const double dx = points[i][0] - centroids[(size_t) c][0], dy = points[i][1] - centroids[(size_t) c][1];
                    if (dx * dx + dy * dy < closestDistance)
                    {
                        closestDistance = dx * dx + dy * dy;
                        closest = c;
                    }
                }
                misplaced += closest != labels[i] ? 1 : 0;
            }
            expectEquals (misplaced, 0);
        }

        beginTest ("Projection can be stopped");
        {
            std::vector<std::vector<float>> data (50, std::vector<float> (8, 0.0f));
            for (size_t i = 0; i < data.size(); ++i)
                data[i][i % 8] = (float) i;
            expect (SourceTsne::project (data, {}, [] { return true; }).empty());
        }
    }
};


class FreesoundSearchTests : public juce::UnitTest
{
public:
    FreesoundSearchTests() : juce::UnitTest ("Freesound search", "Source") {}

    void runTest() override
    {
        beginTest ("Filters");
        {
            FreesoundSearchOptions options;
            expect (options.getFilter().isEmpty());
            options.license = FreesoundSearchOptions::License::cc0;
            expectEquals (options.getFilter(), juce::String ("license:\"Creative Commons 0\""));
            options.license = FreesoundSearchOptions::License::cc0OrAttribution;
            options.minDuration = 0.5f;
            options.maxDuration = 3.0f;
            expectEquals (options.getFilter(), juce::String ("license:(\"Creative Commons 0\" OR \"Attribution\") duration:[0.5 TO 3]"));
            options.license = FreesoundSearchOptions::License::any;
            options.maxDuration = 0.0f;
            expectEquals (options.getFilter(), juce::String ("duration:[0.5 TO *]"));
            options.acousticLevels[0] = FreesoundSearchOptions::Level::low;    // brightness
            options.acousticLevels[6] = FreesoundSearchOptions::Level::high;   // warmth
            options.acousticLevels[2] = FreesoundSearchOptions::Level::mid;    // depth
            expectEquals (options.getFilter(), juce::String ("duration:[0.5 TO *] brightness:[0 TO 33.33] depth:[33.33 TO 66.67] warmth:[66.67 TO 100]"));
        }

        beginTest ("Results are parsed from the API response");
        {
            const auto json = juce::JSON::parse (R"({"id": 372688, "name": "Bassoon", "username": "Samulis", "type": "wav",
                "license": "http://creativecommons.org/publicdomain/zero/1.0/", "duration": 7.22, "filesize": 1273000,
                "previews": {"preview-hq-ogg": "https://cdn.freesound.org/previews/372/372688_2475994-hq.ogg"},
                "tristimulus": [0.2, 0.6, 0.2], "sim_laion_clap": [0.1, -0.2, 0.3]})");
            const auto result = FreesoundSearchResult::fromJson (json);
            expectEquals (result.id, 372688);
            expectEquals (result.name, juce::String ("Bassoon"));
            expectEquals (result.username, juce::String ("Samulis"));
            expectEquals (result.format, juce::String ("wav"));
            expectWithinAbsoluteError (result.duration, 7.22, 1.0e-6);
            expectEquals ((int) result.filesize, 1273000);
            expectEquals (result.previewURL, juce::String ("https://cdn.freesound.org/previews/372/372688_2475994-hq.ogg"));
            expect (result.hasTristimulus());
            expectEquals ((int) result.embedding.size(), 3);
            // Colours are scaled so the strongest component is at full intensity
            expectEquals ((int) result.getColour().getGreen(), 255);
            expectEquals ((int) result.getColour().getRed(), 85);

            const auto withoutDescriptors = FreesoundSearchResult::fromJson (juce::JSON::parse (R"({"id": 1, "name": "x", "tristimulus": null})"));
            expect (! withoutDescriptors.hasTristimulus());
            expect (withoutDescriptors.embedding.empty());
        }

        beginTest ("Map positions are computed for all results");
        {
            std::vector<FreesoundSearchResult> results (20);
            for (size_t i = 0; i < results.size(); ++i)
            {
                results[i].id = (int) i;
                if (i < 16)  // Some results without embedding
                    results[i].embedding = { (float) (i % 4), (float) (i / 4), 1.0f, 0.5f };
            }
            FreesoundSearch::computeMapPositions (results);
            for (const auto& r : results)
            {
                expect (r.hasPosition);
                expect (std::isfinite (r.x) && std::isfinite (r.y));
                expect (std::abs (r.x) <= 1.4f && std::abs (r.y) <= 1.4f);
            }
        }
    }
};


static TsneTests tsneTests;
static FreesoundSearchTests freesoundSearchTests;
