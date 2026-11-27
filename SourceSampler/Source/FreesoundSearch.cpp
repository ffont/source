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
juce::String FreesoundSearchOptions::getFilter() const
{
    juce::StringArray filters;
    switch (license)
    {
        case License::any: break;
        case License::cc0: filters.add ("license:\"Creative Commons 0\""); break;
        case License::cc0OrAttribution: filters.add ("license:(\"Creative Commons 0\" OR \"Attribution\")"); break;
        case License::noNonCommercial: filters.add ("-license:\"Attribution NonCommercial\""); break;
    }
    if (minDuration > 0.0f || maxDuration > 0.0f)
        filters.add ("duration:[" + juce::String (minDuration) + " TO " + (maxDuration > 0.0f ? juce::String (maxDuration) : juce::String ("*")) + "]");
    return filters.joinIntoString (" ");
}


//==============================================================================
namespace
{
    /** Runs a search (in a pool thread). The HTTP request can be aborted from other threads with cancel(). */
    class SearchJob : public juce::ThreadPoolJob
    {
    public:
        using Callback = std::function<void (std::shared_ptr<std::vector<FreesoundSearchResult>>, int total, const juce::String& error)>;
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
                        onFinished (nullptr, 0, error);
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
            FreesoundSearch::computeMapPositions (*results, shouldStop);
            if (shouldStop())
                return jobHasFinished;
            onFinished (results, total, {});
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
        [weak, generation] (std::shared_ptr<std::vector<FreesoundSearchResult>> newResults, int total, const juce::String& error)
        {
            juce::MessageManager::callAsync ([weak, generation, newResults, total, error]
            {
                if (auto* search = weak.get())
                    search->searchFinished (generation, newResults, total, error);
            });
        }), true);
}

void FreesoundSearch::searchFinished (int generation, std::shared_ptr<std::vector<FreesoundSearchResult>> newResults, int total, const juce::String& error)
{
    if (currentGeneration->load() != generation)
        return;
    if (error.isNotEmpty() || newResults == nullptr)
    {
        setStatus (Status::error, error.isNotEmpty() ? error : juce::String ("Search failed"));
        return;
    }
    results = newResults;
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

void FreesoundSearch::computeMapPositions (std::vector<FreesoundSearchResult>& results, const std::function<bool()>& shouldStop)
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

    const auto points = SourceTsne::project (std::move (data), {}, shouldStop);
    if (points.size() == withEmbedding.size())
    {
        for (size_t k = 0; k < withEmbedding.size(); ++k)
        {
            auto& r = results[withEmbedding[k]];
            r.x = points[k][0];
            r.y = points[k][1];
            r.hasPosition = true;
        }
    }
    // Sounds without embedding go in a ring around the others
    for (size_t k = 0; k < withoutEmbedding.size(); ++k)
    {
        auto& r = results[withoutEmbedding[k]];
        const float angle = juce::MathConstants<float>::twoPi * (float) k / (float) juce::jmax ((size_t) 1, withoutEmbedding.size());
        r.x = 1.15f * std::cos (angle);
        r.y = 1.15f * std::sin (angle);
        r.hasPosition = true;
    }
}
