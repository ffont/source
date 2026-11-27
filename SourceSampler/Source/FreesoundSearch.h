/*
  ==============================================================================

    FreesoundSearch.h

    Freesound text searches for the "Search" tab of the native UI. Searches run
    in a background thread: results (up to a maximum number, fetched in pages
    of 150) include the sounds' tristimulus (used to colour them) and their
    LAION-CLAP embeddings, which are projected to 2D with t-SNE to place similar
    sounds close to each other in the sounds map.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <array>
#include <memory>
#include <vector>


struct FreesoundSearchResult
{
    int id = -1;
    juce::String name, username, license, previewURL, format;
    double duration = 0.0;
    juce::int64 filesize = 0;
    std::array<float, 3> tristimulus { -1.0f, -1.0f, -1.0f };  // -1 if not available
    std::vector<float> embedding;                              // sim_laion_clap (empty if not available)
    float x = 0.0f, y = 0.0f;                                  // Position in the map, in [-1, 1]
    bool hasPosition = false;

    bool hasTristimulus() const { return tristimulus[0] >= 0.0f; }
    /** Colour from the tristimulus (scaled so its maximum component is 1), grey if not available. */
    juce::Colour getColour() const;
    /** Parses a result of the Freesound API (one element of "results"). */
    static FreesoundSearchResult fromJson (const juce::var& json);
};


struct FreesoundSearchOptions
{
    enum class License { any, cc0, cc0OrAttribution, noNonCommercial };

    juce::String query;
    License license = License::any;
    float minDuration = 0.0f;
    float maxDuration = 0.0f;   // 0 means no maximum
    int maxResults = 150;

    /** The "filter" parameter of the search request. */
    juce::String getFilter() const;
};


class FreesoundSearch : public juce::ChangeBroadcaster
{
public:
    enum class Status { idle, searching, projecting, done, error };

    static constexpr int pageSize = 150;
    static constexpr int maxResultsLimit = 600;
    static juce::String getFields() { return "id,name,username,license,duration,filesize,type,previews,tristimulus,sim_laion_clap"; }

    explicit FreesoundSearch (juce::String apiKey);
    ~FreesoundSearch() override;

    /** Starts a search (message thread). A search in progress is abandoned (its results are ignored). Listeners are
        notified (in the message thread) when the status or the results change. */
    void search (const FreesoundSearchOptions& options);

    Status getStatus() const { return status; }
    juce::String getStatusMessage() const { return statusMessage; }
    int getTotalNumResults() const { return totalNumResults; }  // Number of sounds matching the query in Freesound
    const FreesoundSearchOptions& getLastOptions() const { return lastOptions; }
    std::shared_ptr<const std::vector<FreesoundSearchResult>> getResults() const { return results; }
    /** Replaces the results (e.g. for tests). Listeners are notified. */
    void setResults (std::shared_ptr<const std::vector<FreesoundSearchResult>> newResults)
    {
        results = std::move (newResults);
        setStatus (Status::done, juce::String ((int) results->size()) + " sounds");
    }

    /** Gets sounds similar to a Freesound sound (in a background thread). The callback is called in the message thread
        (only if this object still exists), with an error message if the request failed. */
    using SimilarSoundsCallback = std::function<void (std::vector<FreesoundSearchResult> results, juce::String error)>;
    void findSimilarSounds (int soundId, int numSounds, SimilarSoundsCallback callback);

    /** Adds 2D positions to the results using t-SNE on their embeddings (results without embedding are placed
        around the others). Can be called from any thread. */
    static void computeMapPositions (std::vector<FreesoundSearchResult>& results, const std::function<bool()>& shouldStop = nullptr);

private:
    void setStatus (Status newStatus, const juce::String& message);
    void cancelJobs();
    void searchFinished (int generation, std::shared_ptr<std::vector<FreesoundSearchResult>> newResults, int total, const juce::String& error);

    juce::String apiKey;
    Status status = Status::idle;
    juce::String statusMessage;
    int totalNumResults = 0;
    FreesoundSearchOptions lastOptions;
    std::shared_ptr<const std::vector<FreesoundSearchResult>> results = std::make_shared<std::vector<FreesoundSearchResult>>();
    std::shared_ptr<std::atomic<int>> currentGeneration = std::make_shared<std::atomic<int>> (0);
    juce::ThreadPool pool { juce::ThreadPoolOptions().withThreadName ("FreesoundSearch").withNumberOfThreads (2) };

    JUCE_DECLARE_WEAK_REFERENCEABLE (FreesoundSearch)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FreesoundSearch)
};
