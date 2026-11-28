/*
  ==============================================================================

    FreesoundSearch.cpp

  ==============================================================================
*/

#include "FreesoundSearch.h"
#include "Tsne.h"


//==============================================================================
juce::Colour FreesoundSearchResult::getColour() const
{
    if (! hasTristimulus())
        return juce::Colour (0xff9a9a9a);
    const float maxComponent = juce::jmax (tristimulus[0], tristimulus[1], tristimulus[2], 1.0e-6f);
    auto component = [maxComponent] (float v) { return (juce::uint8) juce::jlimit (0, 255, juce::roundToInt (255.0f * v / maxComponent)); };
    return juce::Colour (component (tristimulus[0]), component (tristimulus[1]), component (tristimulus[2]));
}

FreesoundSearchResult FreesoundSearchResult::fromJson (const juce::var& json)
{
    FreesoundSearchResult result;
    result.id = json.getProperty ("id", -1);
    result.name = json.getProperty ("name", "").toString();
    result.username = json.getProperty ("username", "").toString();
    result.license = json.getProperty ("license", "").toString();
    result.format = json.getProperty ("type", "").toString();
    result.duration = json.getProperty ("duration", 0.0);
    result.filesize = (juce::int64) json.getProperty ("filesize", 0);
    result.previewURL = json.getProperty ("previews", juce::var()).getProperty ("preview-hq-ogg", "").toString();

    const auto tristimulus = json.getProperty ("tristimulus", juce::var());
    if (tristimulus.isArray() && tristimulus.size() == 3)
        for (int i = 0; i < 3; ++i)
            result.tristimulus[(size_t) i] = juce::jlimit (0.0f, 1.0f, (float) tristimulus[i]);

    const auto onsets = json.getProperty ("onset_times", juce::var());
    if (onsets.isArray())
    {
        for (int i = 0; i < onsets.size(); ++i)
            result.onsetTimes.push_back ((float) onsets[i]);
    }
    else if (onsets.isDouble() || onsets.isInt())
    {
        result.onsetTimes.push_back ((float) onsets);  // A single onset may come as a number
    }

    const auto embedding = json.getProperty ("sim_laion_clap", juce::var());
    if (embedding.isArray())
    {
        result.embedding.reserve ((size_t) embedding.size());
        for (int i = 0; i < embedding.size(); ++i)
            result.embedding.push_back ((float) embedding[i]);
    }
    return result;
}


//==============================================================================
juce::String FreesoundSearchOptions::getLicenseFilter() const
{
    switch (license)
    {
        case License::any: return {};
        case License::cc0: return "license:\"Creative Commons 0\"";
        case License::cc0OrAttribution: return "license:(\"Creative Commons 0\" OR \"Attribution\")";
        case License::noNonCommercial: return "-license:\"Attribution NonCommercial\"";
    }
    return {};
}

juce::String FreesoundSearchOptions::getAcousticFilter() const
{
    static const std::array<const char*, 3> ranges { "[0 TO 33.33]", "[33.33 TO 66.67]", "[66.67 TO 100]" };
    juce::StringArray filters;
    for (size_t i = 0; i < acousticLevels.size(); ++i)
        if (acousticLevels[i] != Level::any)
            filters.add (getAcousticPropertyNames()[(int) i] + ":" + ranges[(size_t) acousticLevels[i]]);
    return filters.joinIntoString (" ");
}

juce::String FreesoundSearchOptions::getFilter() const
{
    juce::StringArray filters;
    if (getLicenseFilter().isNotEmpty())
        filters.add (getLicenseFilter());
    if (minDuration > 0.0f || maxDuration > 0.0f)
        filters.add ("duration:[" + juce::String (minDuration) + " TO " + (maxDuration > 0.0f ? juce::String (maxDuration) : juce::String ("*")) + "]");
    if (getAcousticFilter().isNotEmpty())
        filters.add (getAcousticFilter());
    return filters.joinIntoString (" ");
}


//==============================================================================
namespace
{
    /** Runs a search (in a pool thread). The HTTP request can be aborted from other threads with cancel(). */
    class SearchJob : public juce::ThreadPoolJob
    {
    public:
        using Callback = std::function<void (std::shared_ptr<std::vector<FreesoundSearchResult>>, std::shared_ptr<MapAnimationFrames>, int total, const juce::String& error)>;
        using ProgressCallback = std::function<void (const juce::String& message)>;

        SearchJob (juce::String key, FreesoundSearchOptions o, std::function<bool()> isCurrent, ProgressCallback p, Callback c)
            : juce::ThreadPoolJob ("FreesoundSearch"), apiKey (std::move (key)), options (std::move (o)),
              isStillCurrent (std::move (isCurrent)), onProgress (std::move (p)), onFinished (std::move (c)) {}

        void cancel()
        {
            const juce::ScopedLock sl (streamLock);
            cancelled = true;
            if (currentStream != nullptr)
                currentStream->cancel();
        }

        JobStatus runJob() override
        {
            auto shouldStop = [this] { return shouldExit() || cancelled.load() || ! isStillCurrent(); };
            auto results = std::make_shared<std::vector<FreesoundSearchResult>>();
            const int maxResults = juce::jlimit (1, FreesoundSearch::maxResultsLimit, options.maxResults);
            int total = 0;

            juce::URL url = juce::URL ("https://freesound.org/apiv2/search/text/")
                                .withParameter ("query", options.query)
                                .withParameter ("fields", FreesoundSearch::getFields())
                                .withParameter ("page_size", juce::String (juce::jmin (FreesoundSearch::pageSize, maxResults)));
            if (options.getFilter().isNotEmpty())
                url = url.withParameter ("filter", options.getFilter());

            while ((int) results->size() < maxResults)
            {
                juce::String error;
                const auto json = fetchJson (url.withParameter ("token", apiKey), error, shouldStop);
                if (shouldStop())
                    return jobHasFinished;
                if (error.isNotEmpty())
                {
                    // Keep the results of the previous pages, if any
                    if (results->empty())
                    {
                        onFinished (nullptr, nullptr, 0, error);
                        return jobHasFinished;
                    }
                    break;
                }
                total = json.getProperty ("count", 0);
                const auto pageResults = json.getProperty ("results", juce::var());
                for (int i = 0; i < pageResults.size() && (int) results->size() < maxResults; ++i)
                    results->push_back (FreesoundSearchResult::fromJson (pageResults[i]));
                const auto next = json.getProperty ("next", juce::var()).toString();
                if (next.isEmpty() || pageResults.size() == 0)
                    break;
                url = juce::URL (next);
                onProgress ("Searching... (" + juce::String ((int) results->size()) + " of " + juce::String (juce::jmin (total, maxResults)) + " sounds)");
            }

            onProgress ("Computing map...");
            auto animation = std::make_shared<MapAnimationFrames>();
            FreesoundSearch::computeMapPositions (*results, shouldStop, animation.get());
            if (shouldStop())
                return jobHasFinished;
            // Embeddings are not needed anymore (they are big: 512 values per sound)
            for (auto& r : *results)
                std::vector<float>().swap (r.embedding);
            onFinished (results, animation, total, {});
            return jobHasFinished;
        }

    private:
        juce::var fetchJson (const juce::URL& url, juce::String& error, const std::function<bool()>& shouldStop)
        {
            juce::WebInputStream stream (url, false);
            stream.withConnectionTimeout (60000);
            {
                const juce::ScopedLock sl (streamLock);
                if (cancelled)
                    return {};
                currentStream = &stream;
            }
            juce::String text;
            const bool connected = stream.connect (nullptr);
            const int status = stream.getStatusCode();
            if (connected && ! shouldStop())
                text = stream.readEntireStreamAsString();
            {
                const juce::ScopedLock sl (streamLock);
                currentStream = nullptr;
            }
            if (shouldStop())
                return {};
            if (! connected || status == 0)
            {
                error = "Could not connect to Freesound";
                return {};
            }
            const auto json = juce::JSON::parse (text);
            if (status >= 400)
            {
                const auto detail = json.getProperty ("detail", "").toString();
                error = "Freesound error " + juce::String (status) + (detail.isNotEmpty() ? ": " + detail : juce::String());
                return {};
            }
            if (! json.isObject())
                error = "Unexpected response from Freesound";
            return json;
        }

        juce::String apiKey;
        FreesoundSearchOptions options;
        std::function<bool()> isStillCurrent;
        ProgressCallback onProgress;
        Callback onFinished;
        juce::CriticalSection streamLock;
        juce::WebInputStream* currentStream = nullptr;
        std::atomic<bool> cancelled { false };
    };
}


//==============================================================================
FreesoundSearch::FreesoundSearch (juce::String key) : apiKey (std::move (key)) {}

FreesoundSearch::~FreesoundSearch()
{
    // Abandon (and abort) the search in progress
    currentGeneration->fetch_add (1);
    cancelJobs();
    pool.removeAllJobs (true, 10000);
}

void FreesoundSearch::cancelJobs()
{
    for (int i = 0; i < pool.getNumJobs(); ++i)
        if (auto* job = dynamic_cast<SearchJob*> (pool.getJob (i)))
            job->cancel();
}

void FreesoundSearch::search (const FreesoundSearchOptions& options)
{
    lastOptions = options;
    if (options.query.trim().isEmpty() && options.getFilter().isEmpty())
    {
        setStatus (Status::error, "Enter something to search");
        return;
    }

    // Abandon the previous search (results of old searches are ignored)
    const int generation = currentGeneration->fetch_add (1) + 1;
    cancelJobs();

    setStatus (Status::searching, "Searching...");
    juce::WeakReference<FreesoundSearch> weak (this);
    auto generationCounter = currentGeneration;
    pool.addJob (new SearchJob (apiKey, options,
        [generationCounter, generation] { return generationCounter->load() == generation; },
        [weak, generation] (const juce::String& message)
        {
            juce::MessageManager::callAsync ([weak, generation, message]
            {
                if (auto* search = weak.get())
                    if (search->currentGeneration->load() == generation)
                        search->setStatus (message.startsWith ("Computing") ? Status::projecting : Status::searching, message);
            });
        },
        [weak, generation] (std::shared_ptr<std::vector<FreesoundSearchResult>> newResults, std::shared_ptr<MapAnimationFrames> animation, int total, const juce::String& error)
        {
            juce::MessageManager::callAsync ([weak, generation, newResults, animation, total, error]
            {
                if (auto* search = weak.get())
                    search->searchFinished (generation, newResults, animation, total, error);
            });
        }), true);
}

void FreesoundSearch::findSimilarSounds (int soundId, int numSounds, SimilarSoundsCallback onFound)
{
    juce::WeakReference<FreesoundSearch> weak (this);
    const auto url = juce::URL ("https://freesound.org/apiv2/sounds/" + juce::String (soundId) + "/similar/")
                         .withParameter ("fields", "id,name,username,license,duration,filesize,type,previews,onset_times")
                         .withParameter ("page_size", juce::String (numSounds))
                         .withParameter ("token", apiKey);
    pool.addJob ([weak, url, onFound]
    {
        std::vector<FreesoundSearchResult> foundSounds;
        juce::String errorMessage;
        int statusCode = 0;
        auto stream = url.createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                                                 .withConnectionTimeoutMs (60000).withStatusCode (&statusCode));
        if (stream == nullptr || statusCode == 0)
        {
            errorMessage = "Could not connect to Freesound";
        }
        else
        {
            const auto json = juce::JSON::parse (stream->readEntireStreamAsString());
            if (statusCode >= 400)
                errorMessage = "Freesound error " + juce::String (statusCode);
            const auto jsonResults = json.getProperty ("results", juce::var());
            for (int i = 0; i < jsonResults.size(); ++i)
                foundSounds.push_back (FreesoundSearchResult::fromJson (jsonResults[i]));
        }
        juce::MessageManager::callAsync ([weak, onFound, sounds = std::move (foundSounds), errorMessage]
        {
            if (weak.get() != nullptr && onFound != nullptr)
                onFound (sounds, errorMessage);
        });
    });
}

namespace
{
    /** Simple blocking request (background threads only). */
    juce::var getJson (const juce::URL& url, juce::String& error)
    {
        int statusCode = 0;
        auto stream = url.createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                                                 .withConnectionTimeoutMs (60000).withStatusCode (&statusCode));
        if (stream == nullptr || statusCode == 0)
        {
            error = "Could not connect to Freesound";
            return {};
        }
        const auto json = juce::JSON::parse (stream->readEntireStreamAsString());
        if (statusCode >= 400)
            error = "Freesound error " + juce::String (statusCode);
        return json;
    }
}

void FreesoundSearch::findRandomSounds (int numSounds, const juce::String& filter, RandomSoundsCallback onFound)
{
    juce::WeakReference<FreesoundSearch> weak (this);
    const auto baseUrl = juce::URL ("https://freesound.org/apiv2/search/text/").withParameter ("token", apiKey).withParameter ("filter", filter);
    pool.addJob ([weak, baseUrl, numSounds, onFound]
    {
        std::vector<FreesoundSearchResult> foundSounds;
        juce::String errorMessage;
        // Number of sounds matching the filter
        const auto countJson = getJson (baseUrl.withParameter ("page_size", "1").withParameter ("fields", "id"), errorMessage);
        const int numMatching = countJson.getProperty ("count", 0);
        if (errorMessage.isEmpty() && numMatching > 0)
        {
            // Random (different) positions of the results, each requested as a page of 1 sound
            std::vector<int> positions;
            auto& random = juce::Random::getSystemRandom();
            const int count = juce::jmin (numSounds, numMatching);
            while ((int) positions.size() < count)
            {
                const int position = random.nextInt (numMatching);
                if (std::find (positions.begin(), positions.end(), position) == positions.end())
                    positions.push_back (position);
            }
            for (int position : positions)
            {
                if (weak.wasObjectDeleted())
                    return;
                juce::String pageError;
                const auto json = getJson (baseUrl.withParameter ("page_size", "1")
                                                  .withParameter ("page", juce::String (position + 1))
                                                  .withParameter ("fields", "id,name,username,license,duration,filesize,type,previews,onset_times"), pageError);
                const auto pageResults = json.getProperty ("results", juce::var());
                if (pageResults.size() > 0)
                    foundSounds.push_back (FreesoundSearchResult::fromJson (pageResults[0]));
            }
        }
        juce::MessageManager::callAsync ([weak, onFound, sounds = std::move (foundSounds), numMatching, errorMessage]
        {
            if (weak.get() != nullptr && onFound != nullptr)
                onFound (sounds, numMatching, errorMessage);
        });
    });
}

void FreesoundSearch::searchFinished (int generation, std::shared_ptr<std::vector<FreesoundSearchResult>> newResults,
                                      std::shared_ptr<MapAnimationFrames> animation, int total, const juce::String& error)
{
    if (currentGeneration->load() != generation)
        return;
    if (error.isNotEmpty() || newResults == nullptr)
    {
        setStatus (Status::error, error.isNotEmpty() ? error : juce::String ("Search failed"));
        return;
    }
    results = newResults;
    mapAnimation = animation;
    totalNumResults = total;
    if (newResults->empty())
        setStatus (Status::done, "No sounds found");
    else
        setStatus (Status::done, juce::String ((int) newResults->size()) + " sounds" + (total > (int) newResults->size() ? " (of " + juce::String (total) + " found)" : juce::String()));
}

void FreesoundSearch::setStatus (Status newStatus, const juce::String& message)
{
    status = newStatus;
    statusMessage = message;
    sendChangeMessage();
}

void FreesoundSearch::computeMapPositions (std::vector<FreesoundSearchResult>& results, const std::function<bool()>& shouldStop,
                                           MapAnimationFrames* animation)
{
    // Only results with embeddings of the most common size are projected
    std::map<size_t, int> sizes;
    for (const auto& r : results)
        if (! r.embedding.empty())
            sizes[r.embedding.size()] += 1;
    size_t embeddingSize = 0;
    int bestCount = 0;
    for (const auto& [size, count] : sizes)
        if (count > bestCount)
        {
            embeddingSize = size;
            bestCount = count;
        }

    std::vector<size_t> withEmbedding, withoutEmbedding;
    std::vector<std::vector<float>> data;
    for (size_t i = 0; i < results.size(); ++i)
    {
        if (embeddingSize > 0 && results[i].embedding.size() == embeddingSize)
        {
            withEmbedding.push_back (i);
            data.push_back (results[i].embedding);
        }
        else
        {
            withoutEmbedding.push_back (i);
        }
    }

    // Intermediate positions of the optimisation are kept to animate the map
    std::vector<std::vector<SourceTsne::Point>> snapshots;
    SourceTsne::Options options;
    options.snapshotInterval = animation != nullptr ? 10 : 0;
    float tsneScale = 1.0f;
    const auto points = SourceTsne::project (std::move (data), options, shouldStop,
                                             [&snapshots] (const std::vector<SourceTsne::Point>& p) { snapshots.push_back (p); }, &tsneScale);
    float referenceRadius = 1.0f;
    if (points.size() == withEmbedding.size() && ! points.empty())
    {
        // Scale so that most points (95%) are within radius 1: a few outliers would otherwise squeeze all the other
        // points in the centre of the map. Outliers are brought closer (up to radius 1.2), keeping their direction.
        std::vector<float> radii;
        for (const auto& p : points)
            radii.push_back (std::sqrt (p[0] * p[0] + p[1] * p[1]));
        auto sorted = radii;
        std::sort (sorted.begin(), sorted.end());
        referenceRadius = juce::jmax (1.0e-6f, sorted[(size_t) juce::jmin ((int) sorted.size() - 1, (int) std::ceil (0.95 * (double) sorted.size()) - 1)]);
        for (size_t k = 0; k < withEmbedding.size(); ++k)
        {
            float x = points[k][0] / referenceRadius, y = points[k][1] / referenceRadius;
            const float radius = radii[k] / referenceRadius;
            if (radius > 1.2f)
            {
                x *= 1.2f / radius;
                y *= 1.2f / radius;
            }
            auto& r = results[withEmbedding[k]];
            r.x = x;
            r.y = y;
            r.hasPosition = true;
        }
    }
    // Sounds without embedding go in a ring around the others
    for (size_t k = 0; k < withoutEmbedding.size(); ++k)
    {
        auto& r = results[withoutEmbedding[k]];
        const float angle = juce::MathConstants<float>::twoPi * (float) k / (float) juce::jmax ((size_t) 1, withoutEmbedding.size());
        r.x = 1.35f * std::cos (angle);
        r.y = 1.35f * std::sin (angle);
        r.hasPosition = true;
    }

    if (animation != nullptr && ! snapshots.empty() && snapshots.front().size() == withEmbedding.size())
    {
        // Frames with the same scale as the final positions (so points start in the centre and spread out), ending
        // with the final positions. Sounds without embedding move from the centre to their place in the ring.
        animation->clear();
        const float scale = tsneScale / referenceRadius;
        for (size_t f = 0; f < snapshots.size(); ++f)
        {
            const float progress = (float) (f + 1) / (float) (snapshots.size() + 1);
            std::vector<std::array<float, 2>> frame (results.size(), std::array<float, 2> { 0.0f, 0.0f });
            for (size_t k = 0; k < withEmbedding.size(); ++k)
            {
                float x = snapshots[f][k][0] * scale, y = snapshots[f][k][1] * scale;
                const float radius = std::sqrt (x * x + y * y);
                if (radius > 1.2f)
                {
                    x *= 1.2f / radius;
                    y *= 1.2f / radius;
                }
                frame[withEmbedding[k]] = { x, y };
            }
            for (auto index : withoutEmbedding)
                frame[index] = { results[index].x * progress, results[index].y * progress };
            animation->push_back (std::move (frame));
        }
        std::vector<std::array<float, 2>> finalFrame;
        for (const auto& r : results)
            finalFrame.push_back ({ r.x, r.y });
        animation->push_back (std::move (finalFrame));
    }
}
