/*
  ==============================================================================

    NetworkTests.cpp - tests that use Freesound (only run when the environment
    variable SOURCE_NETWORK_TESTS=1 is set)

  ==============================================================================
*/

#include <JuceHeader.h>
#include "SourceSampler.h"
#include "TestUtils.h"


class NetworkTests : public juce::UnitTest
{
public:
    NetworkTests() : juce::UnitTest ("Network (Freesound)", "Source") {}

    void runTest() override
    {
        if (juce::SystemStats::getEnvironmentVariable ("SOURCE_NETWORK_TESTS", {}) != "1")
            return;

        TestUtils::TemporaryFolder tmp;
        SourceSampler sampler (tmp.folder);
        sampler.prepareToPlay (48000.0, 256);

        beginTest ("Search returns results with map positions");
        FreesoundSearchOptions options;
        options.query = "piano";
        options.maxDuration = 5.0f;
        options.maxResults = 40;
        sampler.freesoundSearch.search (options);
        expect (TestUtils::runMessageLoopUntil ([&] { return sampler.freesoundSearch.getStatus() == FreesoundSearch::Status::done
                                                              || sampler.freesoundSearch.getStatus() == FreesoundSearch::Status::error; }, 120000));
        logMessage (sampler.freesoundSearch.getStatusMessage());
        auto results = sampler.freesoundSearch.getResults();
        expectGreaterThan ((int) results->size(), 0);
        for (const auto& r : *results)
            expect (r.hasPosition);

        beginTest ("Sounds can be replaced with similar sounds");
        {
            const auto& first = results->front();
            sampler.actionListenerCallback (SourceHelpers::serializeAction (ACTION_ADD_SOUND, { juce::String (first.id), first.name, first.username, first.license,
                                                                                                first.previewURL, "", first.format, juce::String ((int) first.filesize), "", "", "60" }));
            auto preset = [&sampler] { return sampler.state.getChildWithName (SourceIDs::PRESET); };
            const auto uuid = preset().getChild (0).getProperty (SourceIDs::uuid).toString();
            sampler.actionListenerCallback (SourceHelpers::serializeAction (ACTION_REPLACE_SOUND_WITH_SIMILAR, { uuid }));
            expect ((bool) preset().getChild (0).getProperty (SourceIDs::findingSimilar));
            expect (TestUtils::runMessageLoopUntil ([&] { return preset().getChild (0).getProperty (SourceIDs::uuid).toString() != uuid; }, 120000), "Not replaced");
            const int newId = preset().getChild (0).getChildWithName (SourceIDs::SOUND_SAMPLE).getProperty (SourceIDs::soundId);
            logMessage ("Replaced " + juce::String (first.id) + " with similar sound " + juce::String (newId));
            expect (newId != first.id && newId > 0);
            expect (! (bool) preset().getChild (0).getProperty (SourceIDs::findingSimilar, false));
        }

        beginTest ("Sounds can be replaced with random sounds");
        {
            auto preset = [&sampler] { return sampler.state.getChildWithName (SourceIDs::PRESET); };
            auto numSounds = [&preset]
            {
                int n = 0;
                for (const auto& c : preset())
                    if (c.hasType (SourceIDs::SOUND) && ! (bool) c.getProperty (SourceIDs::willBeDeleted, false)) ++n;
                return n;
            };
            const auto start = juce::Time::getMillisecondCounterHiRes();
            sampler.actionListenerCallback (SourceHelpers::serializeAction (ACTION_REPLACE_EXISTING_SOUNDS_RANDOMLY, { "4", "0", "3", "0" }));
            expect (sampler.isQueryingSounds());
            expect (TestUtils::runMessageLoopUntil ([&] { return ! sampler.isQueryingSounds(); }, 180000));
            logMessage ("Random sounds in " + juce::String ((juce::Time::getMillisecondCounterHiRes() - start) / 1000.0, 1) + " s, "
                        + sampler.state.getProperty (SourceIDs::numResultsLastQuery).toString() + " matching");
            expectEquals (numSounds(), 4);
        }

        beginTest ("Acoustic property filters");
        {
            FreesoundSearchOptions filtered;
            filtered.query = "piano";
            filtered.maxResults = 20;
            filtered.acousticLevels[0] = FreesoundSearchOptions::Level::low;  // brightness
            sampler.freesoundSearch.search (filtered);
            expect (TestUtils::runMessageLoopUntil ([&] { return sampler.freesoundSearch.getStatus() == FreesoundSearch::Status::done
                                                                  || sampler.freesoundSearch.getStatus() == FreesoundSearch::Status::error; }, 120000));
            logMessage (sampler.freesoundSearch.getStatusMessage());
            expect (sampler.freesoundSearch.getStatus() == FreesoundSearch::Status::done);
            expectGreaterThan ((int) sampler.freesoundSearch.getResults()->size(), 0);
            expectLessThan (sampler.freesoundSearch.getTotalNumResults(), 4681, "The filter reduces the results");
        }

        beginTest ("Previews are downloaded and played");
        const auto url = results->front().previewURL;
        sampler.actionListenerCallback (SourceHelpers::serializeAction (ACTION_PLAY_SOUND_FILE_FROM_PATH, { url }));
        expect (sampler.getPreviewState() == SourceSampler::PreviewState::loading);
        const auto start = juce::Time::getMillisecondCounterHiRes();
        juce::AudioBuffer<float> buffer (2, 256);
        juce::MidiBuffer midi;
        bool heardSomething = false;
        TestUtils::runMessageLoopUntil ([&]
        {
            sampler.processBlock (buffer, midi);
            heardSomething = heardSomething || buffer.getMagnitude (0, 256) > 0.0f;
            return heardSomething || sampler.getPreviewState() == SourceSampler::PreviewState::failed;
        }, 180000);
        logMessage ("Preview after " + juce::String ((juce::Time::getMillisecondCounterHiRes() - start) / 1000.0, 1) + " s");
        expect (heardSomething, "Preview not played");
    }
};

static NetworkTests networkTests;
