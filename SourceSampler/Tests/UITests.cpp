/*
  ==============================================================================

    UITests.cpp - tests for the sounds map and the search panel (no network)

  ==============================================================================
*/

#include <JuceHeader.h>
#include "UI/SoundMapComponent.h"
#include "UI/SearchPanel.h"
#include "SourceSampler.h"
#include "TestUtils.h"


namespace
{
    juce::MouseEvent makeMouseEvent (juce::Component& component, juce::Point<float> position, juce::ModifierKeys mods = {})
    {
        const auto now = juce::Time::getCurrentTime();
        return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), position, mods,
                                 juce::MouseInputSource::defaultPressure, 0.0f, 0.0f, 0.0f, 0.0f,
                                 &component, &component, now, position, now, 1, false);
    }

    std::shared_ptr<std::vector<FreesoundSearchResult>> makeResults()
    {
        auto results = std::make_shared<std::vector<FreesoundSearchResult>>();
        const std::array<std::pair<float, float>, 3> positions {{ { -0.5f, 0.0f }, { 0.5f, 0.0f }, { 0.0f, 0.5f } }};
        for (int i = 0; i < 3; ++i)
        {
            FreesoundSearchResult r;
            r.id = 1000 + i;
            r.name = "Sound " + juce::String (i);
            r.username = "user";
            r.license = "http://creativecommons.org/publicdomain/zero/1.0/";
            r.format = "wav";
            r.duration = 1.5;
            r.filesize = 12345;
            r.previewURL = "http://127.0.0.1:9/" + juce::String (r.id) + ".ogg";  // Nothing listens there: downloads fail quickly
            r.x = positions[(size_t) i].first;
            r.y = positions[(size_t) i].second;
            r.hasPosition = true;
            r.tristimulus = { 0.1f, 0.2f, 0.7f };
            results->push_back (r);
        }
        return results;
    }
}


class SoundMapTests : public juce::UnitTest
{
public:
    SoundMapTests() : juce::UnitTest ("Sounds map", "Source") {}

    void runTest() override
    {
        SoundMapComponent map;
        map.setSize (400, 400);
        map.setResults (makeResults());
        int clicked = -1;
        map.onSoundClicked = [&clicked] (int index) { clicked = index; };

        beginTest ("Sounds are found at their screen positions");
        for (int i = 0; i < 3; ++i)
            expectEquals (map.getSoundAt (map.getScreenPosition (i)), i);
        expectEquals (map.getSoundAt ({ 5.0f, 5.0f }), -1);
        expectEquals (map.getScreenPosition (1).x - map.getScreenPosition (0).x, 400.0f * 0.36f, "Map fits the component");

        beginTest ("Clicking a sound selects it, dragging doesn't");
        {
            const auto p = map.getScreenPosition (2);
            map.mouseDown (makeMouseEvent (map, p));
            map.mouseUp (makeMouseEvent (map, p));
            expectEquals (clicked, 2);
            clicked = -1;
            map.mouseDown (makeMouseEvent (map, p));
            map.mouseDrag (makeMouseEvent (map, p + juce::Point<float> (30.0f, 0.0f)));
            map.mouseUp (makeMouseEvent (map, p + juce::Point<float> (30.0f, 0.0f)));
            expectEquals (clicked, -1);
            map.resetView();
        }

        beginTest ("Dragging moves the map");
        {
            const auto before = map.getScreenPosition (0);
            map.mouseDown (makeMouseEvent (map, { 100.0f, 100.0f }));
            map.mouseDrag (makeMouseEvent (map, { 150.0f, 120.0f }));
            map.mouseUp (makeMouseEvent (map, { 150.0f, 120.0f }));
            const auto after = map.getScreenPosition (0);
            expectWithinAbsoluteError (after.x - before.x, 50.0f, 0.01f);
            expectWithinAbsoluteError (after.y - before.y, 20.0f, 0.01f);
            map.resetView();
        }

        beginTest ("Zooming keeps the point under the pointer");
        {
            const auto pointer = map.getScreenPosition (1);
            const float distanceBefore = map.getScreenPosition (0).getDistanceFrom (pointer);
            juce::MouseWheelDetails wheel { 0.0f, 0.3f, false, false, false };
            map.mouseWheelMove (makeMouseEvent (map, pointer), wheel);
            expectWithinAbsoluteError (map.getScreenPosition (1).getDistanceFrom (pointer), 0.0f, 0.01f);
            expectGreaterThan (map.getScreenPosition (0).getDistanceFrom (pointer), distanceBefore * 1.3f);
            map.mouseMagnify (makeMouseEvent (map, pointer), 0.5f);
            expectWithinAbsoluteError (map.getScreenPosition (1).getDistanceFrom (pointer), 0.0f, 0.01f);
            map.resetView();
        }

        beginTest ("Alt+drag rotates the map around its centre");
        {
            const auto centre = map.getLocalBounds().getCentre().toFloat();
            const float distance01 = map.getScreenPosition (0).getDistanceFrom (map.getScreenPosition (1));
            const auto alt = juce::ModifierKeys (juce::ModifierKeys::altModifier);
            map.mouseDown (makeMouseEvent (map, centre + juce::Point<float> (100.0f, 0.0f), alt));
            map.mouseDrag (makeMouseEvent (map, centre + juce::Point<float> (0.0f, 100.0f), alt));  // A quarter turn
            map.mouseUp (makeMouseEvent (map, centre + juce::Point<float> (0.0f, 100.0f), alt));
            expectWithinAbsoluteError (map.getScreenPosition (0).getDistanceFrom (map.getScreenPosition (1)), distance01, 0.01f, "Rotation keeps distances");
            const auto p1 = map.getScreenPosition (1) - centre;
            expectWithinAbsoluteError (p1.x, 0.0f, 0.01f, "Sound on the right moved below the centre");
            expectWithinAbsoluteError (p1.y, 400.0f * 0.36f * 0.5f, 0.01f);
            map.mouseDoubleClick (makeMouseEvent (map, { 5.0f, 5.0f }));
            expectWithinAbsoluteError (map.getScreenPosition (1).y - centre.y, 0.0f, 0.01f, "Double click resets the view");
        }
    }
};


class SearchPanelTests : public juce::UnitTest
{
public:
    SearchPanelTests() : juce::UnitTest ("Search panel", "Source") {}

    void runTest() override
    {
        TestUtils::TemporaryFolder tmp;
        SourceSampler sampler (tmp.folder);
        sampler.prepareToPlay (48000.0, 256);
        auto preset = [&sampler] { return sampler.state.getChildWithName (SourceIDs::PRESET); };
        SourceActionSender actions { [&sampler] (const juce::String& message) { sampler.actionListenerCallback (message); } };
        SearchPanel panel (sampler.freesoundSearch, actions, preset);
        panel.setSize (1000, 600);
        sampler.freesoundSearch.setResults (makeResults());
        TestUtils::runMessageLoopFor (50);  // Change messages are asynchronous

        beginTest ("Results can be selected and added to the preset");
        {
            panel.selectResult (1, false);
            expectEquals (panel.getSelectedIndex(), 1);
            panel.addResultToPreset (1);
            auto sound = preset().getChild (0);
            expect (sound.hasType (SourceIDs::SOUND));
            auto sample = sound.getChildWithName (SourceIDs::SOUND_SAMPLE);
            expectEquals ((int) sample.getProperty (SourceIDs::soundId), 1001);
            expectEquals (sample.getProperty (SourceIDs::name).toString(), juce::String ("Sound 1"));
            expectEquals (sample.getProperty (SourceIDs::previewURL).toString(), juce::String ("http://127.0.0.1:9/1001.ogg"));
            expect ((bool) sample.getProperty (SourceIDs::soundFromFreesound));
            expect (juce::BigInteger().toString (16) != sound.getProperty (SourceIDs::midiNotes).toString(), "Notes were assigned");
        }

        beginTest ("New results reset the selection");
        {
            sampler.freesoundSearch.setResults (makeResults());
            TestUtils::runMessageLoopFor (50);
            expectEquals (panel.getSelectedIndex(), -1);
        }
        // Let the (failing) download finish before the sampler is destroyed
        TestUtils::runMessageLoopUntil ([&] { return (bool) preset().getChild (0).getProperty (SourceIDs::allSoundsLoaded); }, 20000);
    }
};


static SoundMapTests soundMapTests;
static SearchPanelTests searchPanelTests;
