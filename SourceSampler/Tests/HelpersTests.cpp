/*
  ==============================================================================

    HelpersTests.cpp - tests for pure helper functions (no audio, no threads)

  ==============================================================================
*/

#include <JuceHeader.h>
#include "helpers_source.h"
#include "SourceParameters.h"


class NoteLayoutTests : public juce::UnitTest
{
public:
    NoteLayoutTests() : juce::UnitTest ("Note layouts", "Source") {}

    void runTest() override
    {
        beginTest ("Contiguous layout splits the keyboard in equal ranges");
        {
            const int numSounds = 4;
            juce::BigInteger allNotes;
            for (int i = 0; i < numSounds; ++i)
            {
                auto layout = SourceHelpers::computeNoteLayout (NOTE_MAPPING_TYPE_CONTIGUOUS, i, numSounds);
                expectEquals (layout.midiNotes.countNumberOfSetBits(), 32);
                expectEquals (layout.midiNotes.findNextSetBit (0), i * 32);
                expectEquals (layout.midiRootNote, i * 32 + 16);
                expectEquals (layout.midiChannel, SourceDefaults::midiChannel);
                expect ((allNotes & layout.midiNotes).isZero(), "Ranges should not overlap");
                allNotes |= layout.midiNotes;
            }
            expectEquals (allNotes.countNumberOfSetBits(), 128);
        }

        beginTest ("Contiguous layout with more sounds than notes does not produce empty or invalid mappings");
        {
            auto layout = SourceHelpers::computeNoteLayout (NOTE_MAPPING_TYPE_CONTIGUOUS, 200, 300);
            expect (layout.midiNotes.countNumberOfSetBits() >= 1);
            expect (layout.midiNotes.getHighestBit() <= 127);
            expect (layout.midiRootNote <= 127);
        }

        beginTest ("Interleaved layout assigns consecutive notes to consecutive sounds");
        {
            const int numSounds = 3;
            for (int i = 0; i < numSounds; ++i)
            {
                auto layout = SourceHelpers::computeNoteLayout (NOTE_MAPPING_TYPE_INTERLEAVED, i, numSounds);
                expectEquals (layout.midiRootNote, NOTE_MAPPING_INTERLEAVED_ROOT_NOTE + i);
                expect (layout.midiNotes[NOTE_MAPPING_INTERLEAVED_ROOT_NOTE + i]);
                expect (layout.midiNotes[NOTE_MAPPING_INTERLEAVED_ROOT_NOTE + i + numSounds]);
                expect (layout.midiNotes[NOTE_MAPPING_INTERLEAVED_ROOT_NOTE + i - numSounds]);
                expect (! layout.midiNotes[NOTE_MAPPING_INTERLEAVED_ROOT_NOTE + i + 1]);
            }
        }

        beginTest ("'All' layout maps the whole keyboard and uses one MIDI channel per sound");
        {
            for (int i = 0; i < 20; ++i)
            {
                auto layout = SourceHelpers::computeNoteLayout (NOTE_MAPPING_TYPE_ALL, i, 20);
                expectEquals (layout.midiNotes.countNumberOfSetBits(), 128);
                expectEquals (layout.midiChannel, (i % 16) + 1);
            }
        }

        beginTest ("Invalid arguments return an empty mapping");
        {
            expect (SourceHelpers::computeNoteLayout (NOTE_MAPPING_TYPE_CONTIGUOUS, 0, 0).midiNotes.isZero());
            expect (SourceHelpers::computeNoteLayout (NOTE_MAPPING_TYPE_CONTIGUOUS, 5, 2).midiNotes.isZero());
        }
    }
};


class NoteIndexTests : public juce::UnitTest
{
public:
    NoteIndexTests() : juce::UnitTest ("Note index in mapping", "Source") {}

    void runTest() override
    {
        juce::BigInteger notes;
        notes.setBit (48);  // C3
        notes.setBit (49);  // C#3
        notes.setBit (52);  // E3

        beginTest ("Mapped notes return their index ignoring gaps");
        expectEquals (SourceHelpers::noteIndexInMapping (notes, 48), 0);
        expectEquals (SourceHelpers::noteIndexInMapping (notes, 49), 1);
        expectEquals (SourceHelpers::noteIndexInMapping (notes, 52), 2);

        beginTest ("Unmapped notes return the index of the closest mapped note");
        expectEquals (SourceHelpers::noteIndexInMapping (notes, 30), 0);
        expectEquals (SourceHelpers::noteIndexInMapping (notes, 51), 2);
        expectEquals (SourceHelpers::noteIndexInMapping (notes, 100), 2);

        beginTest ("Empty mapping returns 0");
        expectEquals (SourceHelpers::noteIndexInMapping ({}, 60), 0);
    }
};


class StateHelpersTests : public juce::UnitTest
{
public:
    StateHelpersTests() : juce::UnitTest ("State helpers", "Source") {}

    void runTest() override
    {
        beginTest ("Default empty state has a preset with default values");
        {
            auto state = SourceHelpers::createDefaultEmptyState();
            expect (state.hasType (SourceIDs::SOURCE_STATE));
            expect (state.hasProperty (SourceIDs::uuid));
            auto preset = state.getChildWithName (SourceIDs::PRESET);
            expect (preset.isValid());
            expectEquals ((int) preset.getProperty (SourceIDs::numVoices), SourceDefaults::numVoices);
            expectEquals (preset.getNumChildren(), 0);
        }

        beginTest ("Sounds are created with all parameters and one sample");
        {
            juce::BigInteger notes;
            notes.setRange (10, 5, true);
            auto sound = SourceHelpers::createSourceSoundAndSourceSamplerSoundFromProperties (
                1234, "name", "user", "license", "https://example.org/1234.ogg", "", "wav", 1000, { "0.1", "0.5" }, notes, 12, 0, 3);
            expect (sound.hasType (SourceIDs::SOUND));
            expectEquals ((int) sound.getProperty (SourceIDs::midiChannel), 3);
            for (const auto& info : getSoundParametersInfo())
                expect (sound.hasProperty (info.getIdentifier()), juce::String ("Missing parameter ") + info.name);

            juce::BigInteger parsedNotes;
            parsedNotes.parseString (sound.getProperty (SourceIDs::midiNotes).toString(), 16);
            expect (parsedNotes == notes);

            auto sample = sound.getChildWithName (SourceIDs::SOUND_SAMPLE);
            expect (sample.isValid());
            expectEquals ((int) sample.getProperty (SourceIDs::soundId), 1234);
            expectEquals ((int) sample.getProperty (SourceIDs::midiRootNote), 12);
            expect ((bool) sample.getProperty (SourceIDs::soundFromFreesound));
            expectEquals (sample.getChildWithName (SourceIDs::ANALYSIS).getChildWithName (SourceIDs::onsets).getNumChildren(), 2);
        }

        beginTest ("Action messages are serialized with the expected separators");
        expectEquals (SourceHelpers::serializeAction (ACTION_SET_POLYPHONY, { "4" }), juce::String ("/set_polyphony:4"));
        expectEquals (SourceHelpers::serializeAction (ACTION_NOTE_ON, { "60", "127", "1" }), juce::String ("/note_on:60;127;1"));

        beginTest ("Parameter metadata is consistent");
        for (const auto& info : getSoundParametersInfo())
        {
            expect (info.minValue < info.maxValue, info.name);
            expect (info.defaultValue >= info.minValue && info.defaultValue <= info.maxValue, info.name);
            expect (findSoundParameterInfo (info.name) == &info);
        }
        expect (findSoundParameterInfo ("doesNotExist") == nullptr);
    }
};


class MpeHelpersTests : public juce::UnitTest
{
public:
    MpeHelpersTests() : juce::UnitTest ("MPE helpers", "Source") {}

    void runTest() override
    {
        beginTest ("Source names round-trip");
        for (const auto& name : SourceHelpers::getMpeSourceNames())
            expect (SourceHelpers::mpeSourceFromName (name) != SourceHelpers::MpeSource::numSources);
        expect (SourceHelpers::mpeSourceFromName ("nope") == SourceHelpers::MpeSource::numSources);

        beginTest ("Linear curve is the identity and curves keep the end points");
        for (float x : { 0.0f, 0.25f, 0.5f, 1.0f })
            expectWithinAbsoluteError (SourceHelpers::applyMpeCurve (x, 0.0f, false), x, 1.0e-6f);
        for (float curve : { -1.0f, -0.3f, 0.5f, 1.0f })
        {
            expectWithinAbsoluteError (SourceHelpers::applyMpeCurve (0.0f, curve, false), 0.0f, 1.0e-6f);
            expectWithinAbsoluteError (SourceHelpers::applyMpeCurve (1.0f, curve, false), 1.0f, 1.0e-6f);
        }

        beginTest ("Positive curves start slow, negative curves start fast");
        expectLessThan (SourceHelpers::applyMpeCurve (0.5f, 0.5f, false), 0.5f);
        expectGreaterThan (SourceHelpers::applyMpeCurve (0.5f, -0.5f, false), 0.5f);

        beginTest ("Bipolar curves are symmetric around the centre");
        expectWithinAbsoluteError (SourceHelpers::applyMpeCurve (0.5f, 0.8f, true), 0.5f, 1.0e-6f);
        const float up = SourceHelpers::applyMpeCurve (0.75f, 0.8f, true) - 0.5f;
        const float down = 0.5f - SourceHelpers::applyMpeCurve (0.25f, 0.8f, true);
        expectWithinAbsoluteError (up, down, 1.0e-6f);

        beginTest ("Values are mapped to the mapping range and then to the parameter range");
        expectWithinAbsoluteError (SourceHelpers::mapMpeValueToParameter (1.0f, 0.0f, false, 0.0f, 0.5f, -80.0f, 12.0f), -34.0f, 1.0e-4f);
        expectWithinAbsoluteError (SourceHelpers::mapMpeValueToParameter (0.0f, 0.0f, false, 1.0f, 0.0f, 0.0f, 100.0f), 100.0f, 1.0e-4f);  // Inverted ranges
    }
};


static NoteLayoutTests noteLayoutTests;
static MpeHelpersTests mpeHelpersTests;
static NoteIndexTests noteIndexTests;
static StateHelpersTests stateHelpersTests;
