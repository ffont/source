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

        beginTest ("Preset names are read from preset files (current and old format)");
        {
            auto folder = juce::File::createTempFile ("presetNames");
            folder.createDirectory();
            auto current = folder.getChildFile ("1.xml");
            auto preset = SourceHelpers::createEmptyPresetState();
            preset.setProperty (SourceIDs::name, "My \"preset\" & co", nullptr);
            juce::ValueTree sound ("SOUND");
            sound.setProperty ("name", "not this one", nullptr);
            preset.appendChild (sound, nullptr);
            preset.createXml()->writeTo (current);
            expectEquals (SourceHelpers::readPresetNameFromFile (current), juce::String ("My \"preset\" & co"));

            auto old = folder.getChildFile ("2.xml");
            old.replaceWithText ("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\n<SourcePresetState presetName=\"percussion\" presetNumber=\"1\">\n  <Sampler NumVoices=\"16\"/>\n</SourcePresetState>\n");
            expectEquals (SourceHelpers::readPresetNameFromFile (old), juce::String ("percussion"));

            auto other = folder.getChildFile ("3.xml");
            other.replaceWithText ("<SomethingElse name=\"x\"/>");
            expect (SourceHelpers::readPresetNameFromFile (other).isEmpty());
            other.replaceWithText ("not xml at all");
            expect (SourceHelpers::readPresetNameFromFile (other).isEmpty());
            expect (SourceHelpers::readPresetNameFromFile (folder.getChildFile ("missing.xml")).isEmpty());
            folder.deleteRecursively();
        }

        beginTest ("Old Freesound preview URLs are rewritten to the CDN");
        {
            const juce::String cdn ("https://cdn.freesound.org/previews/372/372688_2475994-hq.ogg");
            expectEquals (SourceHelpers::fixFreesoundPreviewURL ("https://freesound.org/data/previews/372/372688_2475994-hq.ogg"), cdn);
            expectEquals (SourceHelpers::fixFreesoundPreviewURL ("http://www.freesound.org/data/previews/372/372688_2475994-hq.ogg"), cdn);
            expectEquals (SourceHelpers::fixFreesoundPreviewURL (cdn), cdn);
            expectEquals (SourceHelpers::fixFreesoundPreviewURL ("https://example.org/data/previews/1.ogg"), juce::String ("https://example.org/data/previews/1.ogg"));
            expectEquals (SourceHelpers::fixFreesoundPreviewURL (""), juce::String());

            auto file = juce::File::createTempFile ("xml");
            file.replaceWithText ("<PRESET name=\"p\"><SOUND><SOUND_SAMPLE previewURL=\"https://freesound.org/data/previews/1/1_2-hq.ogg\"/></SOUND>"
                                  "<soundInfo soundOGGURL=\"https://freesound.org/data/previews/3/3_4-hq.ogg\"/></PRESET>");
            expect (SourceHelpers::fixFreesoundPreviewURLsInFile (file));
            const auto fixed = file.loadFileAsString();
            expect (fixed.contains ("previewURL=\"https://cdn.freesound.org/previews/1/1_2-hq.ogg\""));
            expect (fixed.contains ("soundOGGURL=\"https://cdn.freesound.org/previews/3/3_4-hq.ogg\""));
            expect (! fixed.contains ("/data/previews/"));
            const auto modificationTime = file.getLastModificationTime();
            expect (! SourceHelpers::fixFreesoundPreviewURLsInFile (file), "Files without old URLs are not rewritten");
            expect (file.getLastModificationTime() == modificationTime);
            file.deleteFile();
        }

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



class GraphHelpersTests : public juce::UnitTest
{
public:
    GraphHelpersTests() : juce::UnitTest ("Graph helpers", "Source") {}

    // Gain of juce::dsp::LadderFilter (as configured in the voices) for a quiet sine, measured after it settles
    static double measureLadderGain (double frequency, float cutoff, float resonance, double sampleRate)
    {
        juce::dsp::LadderFilter<float> filter;
        filter.prepare ({ sampleRate, 512, 1 });
        filter.setCutoffFrequencyHz (cutoff);
        filter.setResonance (resonance);
        filter.reset();
        const int numSamples = (int) sampleRate;  // 1 s
        juce::AudioBuffer<float> buffer (1, numSamples);
        for (int i = 0; i < numSamples; ++i)
            buffer.setSample (0, i, 0.001f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * frequency * i / sampleRate));
        juce::dsp::AudioBlock<float> block (buffer);
        filter.process (juce::dsp::ProcessContextReplacing<float> (block));
        const int start = numSamples / 2;
        return buffer.getRMSLevel (0, start, numSamples - start) / (0.001 / std::sqrt (2.0));
    }

    void runTest() override
    {
        using namespace SourceHelpers;
        beginTest ("The filter response matches the ladder filter used by the voices");
        const double sampleRate = 48000.0;
        for (float resonance : { 0.0f, 0.5f, 0.9f })
            for (double frequency : { 100.0, 800.0, 1000.0, 2500.0, 8000.0 })
            {
                const double expected = juce::Decibels::gainToDecibels (measureLadderGain (frequency, 1000.0f, resonance, sampleRate));
                const double computed = juce::Decibels::gainToDecibels (ladderFilterMagnitude (frequency, 1000.0, resonance, sampleRate));
                expectWithinAbsoluteError (computed, expected, 0.5, "f=" + juce::String (frequency) + " res=" + juce::String (resonance));
            }

        beginTest ("ADSR levels");
        expectWithinAbsoluteError (adsrLevelAt (0.05f, 0.1f, 0.2f, 0.5f, 0.3f, 1.0f), 0.5f, 1.0e-5f);   // Half of the attack
        expectWithinAbsoluteError (adsrLevelAt (0.2f, 0.1f, 0.2f, 0.5f, 0.3f, 1.0f), 0.75f, 1.0e-5f);   // Half of the decay
        expectWithinAbsoluteError (adsrLevelAt (0.8f, 0.1f, 0.2f, 0.5f, 0.3f, 1.0f), 0.5f, 1.0e-5f);    // Sustain
        expectWithinAbsoluteError (adsrLevelAt (1.15f, 0.1f, 0.2f, 0.5f, 0.3f, 1.0f), 0.25f, 1.0e-5f);  // Half of the release
        expectWithinAbsoluteError (adsrLevelAt (2.0f, 0.1f, 0.2f, 0.5f, 0.3f, 1.0f), 0.0f, 1.0e-5f);
        expectWithinAbsoluteError (adsrLevelAt (0.0f, 0.0f, 0.0f, 0.7f, 0.0f, 1.0f), 0.7f, 1.0e-5f);    // No attack or decay
    }
};

class ModulatorHelpersTests : public juce::UnitTest
{
public:
    ModulatorHelpersTests() : juce::UnitTest ("Modulator helpers", "Source") {}

    void runTest() override
    {
        using namespace SourceHelpers;

        beginTest ("Modulator type names round trip, invalid names are rejected");
        expect (modulatorTypeFromName (MODULATOR_TYPE_LFO) == ModulatorType::lfo);
        expect (modulatorTypeFromName (MODULATOR_TYPE_ADSR) == ModulatorType::adsr);
        expect (modulatorTypeFromName ("nope") == ModulatorType::numTypes);
        expectEquals (getLfoWaveformNames().size(), (int) numLfoWaveforms);

        beginTest ("LFO waveforms stay in 0..1 and start at their minimum (except square and saw down)");
        for (int waveform = 0; waveform < numLfoWaveforms; ++waveform)
            for (double phase = 0.0; phase < 2.0; phase += 0.01)
            {
                const float v = lfoSignal (waveform, phase, 0.3f);
                expect (v >= 0.0f && v <= 1.0f);
            }
        expectWithinAbsoluteError (lfoSignal (lfoSine, 0.0, 0.0f), 0.0f, 1.0e-6f);
        expectWithinAbsoluteError (lfoSignal (lfoSine, 0.5, 0.0f), 1.0f, 1.0e-6f);
        expectWithinAbsoluteError (lfoSignal (lfoTriangle, 0.25, 0.0f), 0.5f, 1.0e-6f);
        expectWithinAbsoluteError (lfoSignal (lfoSawUp, 0.75, 0.0f), 0.75f, 1.0e-6f);
        expectWithinAbsoluteError (lfoSignal (lfoSawDown, 0.75, 0.0f), 0.25f, 1.0e-6f);
        expectEquals (lfoSignal (lfoSquare, 0.25, 0.0f), 1.0f);
        expectEquals (lfoSignal (lfoSquare, 0.75, 0.0f), 0.0f);
        expectEquals (lfoSignal (lfoRandom, 0.6, 0.3f), 0.3f);

        beginTest ("Envelope: attack, decay, sustain and release");
        {
            ModulatorEnvelope env;
            expectEquals (env.advance (0.1f, 0.2f, 0.2f, 0.5f, 0.4f), 0.0f, "Idle envelopes output 0");
            env.noteOn();
            expectWithinAbsoluteError (env.advance (0.1f, 0.2f, 0.2f, 0.5f, 0.4f), 0.5f, 1.0e-4f);   // Half attack
            expectWithinAbsoluteError (env.advance (0.2f, 0.2f, 0.2f, 0.5f, 0.4f), 0.75f, 1.0e-4f);  // 0.1 s attack + 0.1 s decay
            expectWithinAbsoluteError (env.advance (1.0f, 0.2f, 0.2f, 0.5f, 0.4f), 0.5f, 1.0e-4f);   // Sustain
            env.noteOff();
            expectWithinAbsoluteError (env.advance (0.2f, 0.2f, 0.2f, 0.5f, 0.4f), 0.25f, 1.0e-4f);  // Half release
            expectEquals (env.advance (1.0f, 0.2f, 0.2f, 0.5f, 0.4f), 0.0f);
            expect (env.stage == ModulatorEnvelope::Stage::idle);
        }

        beginTest ("Envelope: note off during the attack releases from the current level; zero times are instantaneous");
        {
            ModulatorEnvelope env;
            env.noteOn();
            env.advance (0.05f, 0.1f, 0.1f, 1.0f, 0.1f);
            env.noteOff();
            expectWithinAbsoluteError (env.advance (0.05f, 0.1f, 0.1f, 1.0f, 0.1f), 0.25f, 1.0e-4f);
            env.noteOn();
            expectWithinAbsoluteError (env.advance (0.001f, 0.0f, 0.0f, 0.3f, 0.0f), 0.3f, 1.0e-4f);
        }

        beginTest ("Sync divisions: longest first, binary, dotted and triplet relations, attack starts with Off");
        {
            const auto& divisions = getSyncDivisions();
            // Grouped by note value (dotted, straight, triplet), straight values from longest to shortest
            double previous = 1000.0;
            for (const auto& d : divisions)
            {
                const juce::String name (d.name);
                if (name.endsWith (" D") || name.endsWith (" T"))
                    continue;
                expectLessThan (d.beats, previous);
                previous = d.beats;
            }
            expectEquals (juce::String (divisions[(size_t) SourceDefaults::lfoSyncDivision].name), juce::String ("1/4"));
            expectEquals (getSyncDivisionBeats (SourceDefaults::lfoSyncDivision), 1.0);
            auto beatsOf = [&divisions] (const juce::String& name)
            {
                for (const auto& d : divisions)
                    if (name == d.name)
                        return d.beats;
                return -1.0;
            };
            expectWithinAbsoluteError (beatsOf ("1/8 T") * 3.0, beatsOf ("1/4"), 1.0e-9, "Three 1/8 triplets in a beat");
            expectWithinAbsoluteError (beatsOf ("1/4 D"), beatsOf ("1/4") * 1.5, 1.0e-9);
            expectEquals (beatsOf ("1 bar"), 4.0);
            expectEquals (getSyncAttackNames()[0], juce::String ("Off"));
            expectEquals (getSyncAttackNames().size(), (int) divisions.size() + 1);
            expectEquals (getSyncAttackBeats (0), 0.0);
            expectEquals (getSyncAttackBeats (1), 32.0);
        }

        beginTest ("Steps: parsing, defaults and the step at each phase");
        {
            expect (modulatorTypeFromName (MODULATOR_TYPE_STEPS) == ModulatorType::steps);
            auto values = parseStepValues ("0.1, 0.9,,2,-1");
            expectWithinAbsoluteError (values[0], 0.1f, 1.0e-6f);
            expectWithinAbsoluteError (values[1], 0.9f, 1.0e-6f);
            expectEquals (values[2], getDefaultStepValues()[2], "Empty values take the default");
            expectEquals (values[3], 1.0f, "Values are clamped");
            expectEquals (values[4], 0.0f);
            expectEquals (values[31], getDefaultStepValues()[31]);
            const auto roundTrip = parseStepValues (formatStepValues (values));
            for (int i = 0; i < maxSteps; ++i)
                expectWithinAbsoluteError (roundTrip[(size_t) i], values[(size_t) i], 1.0e-3f);
            expectEquals (stepsSignal (values, 4, 0.0), values[0]);
            expectEquals (stepsSignal (values, 4, 0.30), values[1]);
            expectEquals (stepsSignal (values, 4, 0.99), values[3]);
            expectEquals (stepsSignal (values, 4, 1.10), values[0], "Cycles repeat");
            expectEquals (stepsSignal (values, 0, 0.7), values[0], "At least one step");
            const auto& lengths = getStepsCycleLengths();
            expectEquals (juce::String (lengths[(size_t) SourceDefaults::stepsSyncLength].name), juce::String ("1 bar"));
            for (size_t i = 1; i < lengths.size(); ++i)
                expectGreaterThan (lengths[i].beats, lengths[i - 1].beats);
        }

        beginTest ("LFO attack: the rate grows exponentially from 0 to 1 during the attack");
        expectEquals (lfoAttackRateFactor (0.3, 0.0), 1.0, "No attack");
        expectEquals (lfoAttackRateFactor (0.0, 1.0), 0.0);
        expectEquals (lfoAttackRateFactor (1.0, 1.0), 1.0);
        expectEquals (lfoAttackRateFactor (5.0, 1.0), 1.0);
        expectLessThan (lfoAttackRateFactor (0.5, 1.0), 0.15, "Slow start (exponential)");
        for (double t = 0.05; t < 1.0; t += 0.05)
            expectGreaterThan (lfoAttackRateFactor (t, 1.0), lfoAttackRateFactor (t - 0.05, 1.0));

        beginTest ("Modulation offsets are added in the normalised range and clamped");
        expectWithinAbsoluteError (applyModulationOffset (-10.0f, 0.5f, -80.0f, 12.0f), 12.0f, 1.0e-4f);
        expectWithinAbsoluteError (applyModulationOffset (0.0f, -0.25f, -36.0f, 36.0f), -18.0f, 1.0e-4f);
        expectWithinAbsoluteError (applyModulationOffset (5.0f, 0.0f, 0.0f, 10.0f), 5.0f, 1.0e-6f);
    }
};

static ModulatorHelpersTests modulatorHelpersTests;
static NoteLayoutTests noteLayoutTests;
static MpeHelpersTests mpeHelpersTests;
static NoteIndexTests noteIndexTests;
static StateHelpersTests stateHelpersTests;
static GraphHelpersTests graphHelpersTests;
