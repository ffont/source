/*
  ==============================================================================

    SamplerRenderTests.cpp - loads real (generated) audio files into the
    synthesiser through SourceSound and checks the rendered audio.

  ==============================================================================
*/

#include <JuceHeader.h>
#include "TestUtils.h"
#include "SourceSamplerSynthesiser.h"
#include "SourceSamplerSound.h"
#include "SourceSamplerVoice.h"


class SamplerRenderTests : public juce::UnitTest
{
public:
    SamplerRenderTests() : juce::UnitTest ("Sampler rendering", "Source") {}

    static constexpr double sampleRate = 44100.0;
    static constexpr int blockSize = 512;

    struct Fixture
    {
        Fixture()
        {
            synth.prepare ({ sampleRate, (juce::uint32) blockSize, 2 });
            synth.setSamplerVoices (8);
            sineFile = TestUtils::writeSineWav (tmp.folder.getChildFile ("sine.wav"), 441.0, 2.0, sampleRate);
            preset = SourceHelpers::createEmptyPresetState();
            sounds = std::make_unique<SourceSoundList> (preset, [this] { return getContext(); });
        }

        GlobalContextStruct getContext()
        {
            GlobalContextStruct context;
            context.sampleRate = sampleRate;
            context.samplesPerBlock = blockSize;
            context.sampler = &synth;
            context.sourceDataLocation = tmp.folder;
            context.soundsDownloadLocation = tmp.folder;
            context.presetFilesLocation = tmp.folder;
            context.tmpFilesLocation = tmp.folder;
            return context;
        }

        /** Adds a non-Freesound sound from a local file mapped to all notes with the given root note. */
        SourceSound* addSound (const juce::File& file, int rootNote)
        {
            juce::BigInteger allNotes;
            allNotes.setRange (0, 128, true);
            auto soundState = SourceHelpers::createSourceSoundAndSourceSamplerSoundFromProperties (
                -1, file.getFileNameWithoutExtension(), "", "", "", file.getFullPathName(), "wav", (int) file.getSize(), {}, allNotes, rootNote, 0, 0);
            preset.addChild (soundState, -1, nullptr);
            return sounds->getSoundWithUUID (soundState.getProperty (SourceIDs::uuid).toString());
        }

        juce::AudioBuffer<float> render (juce::MidiBuffer midi, int numBlocks = 1)
        {
            juce::AudioBuffer<float> output (2, blockSize * numBlocks);
            output.clear();
            for (int b = 0; b < numBlocks; ++b)
            {
                juce::AudioBuffer<float> block (output.getArrayOfWritePointers(), 2, b * blockSize, blockSize);
                juce::MidiBuffer blockMidi;
                if (b == 0)
                    blockMidi = midi;
                synth.renderNextBlock (block, blockMidi, 0, blockSize);
            }
            return output;
        }

        TestUtils::TemporaryFolder tmp;
        SourceSamplerSynthesiser synth;
        juce::File sineFile;
        juce::ValueTree preset;
        std::unique_ptr<SourceSoundList> sounds;
    };

    static juce::MidiBuffer noteOn (int note, int channel = 1)
    {
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (channel, note, (juce::uint8) 127), 0);
        return midi;
    }

    static juce::MidiBuffer noteOff (int note, int channel = 1)
    {
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOff (channel, note), 0);
        return midi;
    }

    void runTest() override
    {
        beginTest ("Sounds are loaded asynchronously and added to the synth");
        {
            Fixture f;
            auto* sound = f.addSound (f.sineFile, 60);
            expect (sound != nullptr);
            expect (TestUtils::runMessageLoopUntil ([&] { return f.synth.getNumSounds() == 1; }), "Sound was not loaded");
            expect ((bool) sound->state.getProperty (SourceIDs::allSoundsLoaded));
            auto sample = sound->state.getChildWithName (SourceIDs::SOUND_SAMPLE);
            expectWithinAbsoluteError ((float) sample.getProperty (SourceIDs::duration), 2.0f, 0.01f);
            expect ((bool) sample.getProperty (SourceIDs::downloadCompleted));
            expect (f.tmp.folder.getChildFile (sample.getProperty (SourceIDs::uuid).toString() + ".wav").existsAsFile(), "Waveform file not written");
        }

        beginTest ("Note on produces audio, root note plays at original pitch and an octave up doubles the frequency");
        {
            Fixture f;
            auto* sound = f.addSound (f.sineFile, 60);
            expect (TestUtils::runMessageLoopUntil ([&] { return f.synth.getNumSounds() == 1; }));
            sound->setParameterByNameFloat (SourceIDs::gain, 0.0f, false);
            // Measure frequency in the steady-state part (skip the first blocks with the attack/filter transients)
            auto steadyState = [] (juce::AudioBuffer<float>& b) { return juce::AudioBuffer<float> (b.getArrayOfWritePointers(), 1, 4 * blockSize, b.getNumSamples() - 4 * blockSize); };
            auto atRoot = f.render (noteOn (60), 24);
            expectGreaterThan (TestUtils::rms (atRoot), 0.1f);
            auto atRootSteady = steadyState (atRoot);
            const int crossingsAtRoot = TestUtils::countPositiveZeroCrossings (atRootSteady);
            expectWithinAbsoluteError (crossingsAtRoot, (int) (441.0 * atRootSteady.getNumSamples() / sampleRate), 2);

            f.synth.allNotesOff (0, false);
            auto octaveUp = f.render (noteOn (72), 24);
            auto octaveUpSteady = steadyState (octaveUp);
            expectWithinAbsoluteError (TestUtils::countPositiveZeroCrossings (octaveUpSteady), crossingsAtRoot * 2, 4);
        }

        beginTest ("Gate mode stops the sound after note off and release");
        {
            Fixture f;
            auto* sound = f.addSound (f.sineFile, 60);
            expect (TestUtils::runMessageLoopUntil ([&] { return f.synth.getNumSounds() == 1; }));
            sound->setParameterByNameInt (SourceIDs::launchMode, LAUNCH_MODE_GATE);
            sound->setParameterByNameFloat (SourceIDs::release, 0.01f, false);
            f.render (noteOn (60), 4);
            f.render (noteOff (60), 4);  // 0.01s release is ~1 block
            auto afterRelease = f.render ({}, 2);
            expectEquals (TestUtils::rms (afterRelease), 0.0f);
        }

        beginTest ("Trigger mode keeps playing after note off");
        {
            Fixture f;
            auto* sound = f.addSound (f.sineFile, 60);
            expect (TestUtils::runMessageLoopUntil ([&] { return f.synth.getNumSounds() == 1; }));
            sound->setParameterByNameInt (SourceIDs::launchMode, LAUNCH_MODE_TRIGGER);
            f.render (noteOn (60), 2);
            f.render (noteOff (60), 2);
            expectGreaterThan (TestUtils::rms (f.render ({}, 2)), 0.01f);
        }

        beginTest ("Loop mode keeps playing past the end of the sound");
        {
            Fixture f;
            auto shortFile = TestUtils::writeSineWav (f.tmp.folder.getChildFile ("short.wav"), 441.0, 0.1, sampleRate);
            auto* sound = f.addSound (shortFile, 60);
            expect (TestUtils::runMessageLoopUntil ([&] { return f.synth.getNumSounds() == 1; }));
            sound->setParameterByNameInt (SourceIDs::launchMode, LAUNCH_MODE_LOOP);
            f.render (noteOn (60), 40);  // ~0.46s, much longer than the sound
            expectGreaterThan (TestUtils::rms (f.render ({}, 4)), 0.05f);
            f.synth.allNotesOff (0, false);
            sound->setParameterByNameInt (SourceIDs::launchMode, LAUNCH_MODE_GATE);
            f.render (noteOn (60), 40);
            expectEquals (TestUtils::rms (f.render ({}, 4)), 0.0f, "Without looping the sound should have ended");
        }

        beginTest ("Reverb with dry level 1 and no wet signal leaves the level unchanged");
        {
            Fixture f;
            auto* sound = f.addSound (f.sineFile, 60);
            expect (TestUtils::runMessageLoopUntil ([&] { return f.synth.getNumSounds() == 1; }));
            sound->setParameterByNameFloat (SourceIDs::gain, 0.0f, false);
            // (Velocity no longer changes the gain unless the sound has a velocity modulator)
            juce::Reverb::Parameters params;
            params.wetLevel = 0.0f;
            params.dryLevel = 1.0f;
            f.synth.setReverbParameters (params);
            auto output = f.render (noteOn (60), 24);
            juce::AudioBuffer<float> steady (output.getArrayOfWritePointers(), 2, 8 * blockSize, output.getNumSamples() - 8 * blockSize);
            // Mono sine of amplitude 0.5 (RMS 0.354) played centered (pan gain 1 on each channel). The voice's ladder
            // filter saturates slightly (~1 dB), but the output must not be boosted (it was +6 dB before compensating
            // for juce::Reverb's internal dry scale factor)
            const float expectedRms = 0.5f / std::sqrt (2.0f);
            const float levelDb = juce::Decibels::gainToDecibels (TestUtils::rms (steady) / expectedRms);
            expect (levelDb > -2.0f && levelDb < 0.5f, "Unexpected level: " + juce::String (levelDb) + " dB");
        }

        beginTest ("Gain parameter changes the output level");
        {
            Fixture f;
            auto* sound = f.addSound (f.sineFile, 60);
            expect (TestUtils::runMessageLoopUntil ([&] { return f.synth.getNumSounds() == 1; }));
            sound->setParameterByNameFloat (SourceIDs::gain, 0.0f, false);
            const float loud = TestUtils::rms (f.render (noteOn (60), 10));
            f.synth.allNotesOff (0, false);
            sound->setParameterByNameFloat (SourceIDs::gain, -40.0f, false);
            const float quiet = TestUtils::rms (f.render (noteOn (60), 10));
            expectGreaterThan (loud, quiet * 20.0f);
        }

        beginTest ("Sounds only respond to their MIDI channel");
        {
            Fixture f;
            auto* sound = f.addSound (f.sineFile, 60);
            expect (TestUtils::runMessageLoopUntil ([&] { return f.synth.getNumSounds() == 1; }));
            sound->setParameterByNameInt (SourceIDs::midiChannel, 2);
            expectEquals (TestUtils::rms (f.render (noteOn (60, 1), 2)), 0.0f);
            expectGreaterThan (TestUtils::rms (f.render (noteOn (60, 2), 2)), 0.01f);
        }

        beginTest ("Time stretch is computed in the background and changes the playback length");
        {
            Fixture f;
            auto* sound = f.addSound (f.sineFile, 60);
            expect (TestUtils::runMessageLoopUntil ([&] { return f.synth.getNumSounds() == 1; }));
            auto* samplerSound = sound->getFirstLinkedSourceSamplerSound();
            const int originalLength = samplerSound->getLengthInSamples();
            // Start a note so the swap happens while a voice is playing
            f.render (noteOn (60), 1);
            sound->setParameterByNameFloat (SourceIDs::timeStretch, 2.0f, false);
            expect (TestUtils::runMessageLoopUntil ([&] {
                f.render ({}, 1);  // Keep rendering while the stretch thread runs
                return samplerSound->getLengthInSamples() != originalLength && ! samplerSound->isComputingStretch(); }, 10000));
            expectWithinAbsoluteError (samplerSound->getLengthInSamples(), originalLength * 2, 4);
            expectEquals (samplerSound->getAudioData()->getNumSamples(), originalLength, "Original data should not change");
        }

        beginTest ("Scheduled sound deletion stops voices and the sound can then be removed");
        {
            Fixture f;
            auto* sound = f.addSound (f.sineFile, 60);
            expect (TestUtils::runMessageLoopUntil ([&] { return f.synth.getNumSounds() == 1; }));
            f.render (noteOn (60), 1);
            sound->scheduleSoundDeletion();
            expect (sound->isScheduledForDeletion());
            expectEquals (TestUtils::rms (f.render (noteOn (60), 1)), 0.0f, "Scheduled sounds should not play");
            expect (TestUtils::runMessageLoopUntil ([&] { return sound->shouldBeDeleted(); }, 2000));
            f.sounds->removeSoundWithUUID (sound->getUUID());
            expectEquals (f.synth.getNumSounds(), 0);
            expectEquals (f.sounds->objects.size(), 0);
        }

        beginTest ("MPE: notes on any channel play regardless of the global/sound MIDI channel");
        {
            Fixture f;
            auto* sound = f.addSound (f.sineFile, 60);
            expect (TestUtils::runMessageLoopUntil ([&] { return f.synth.getNumSounds() == 1; }));
            f.synth.setGlobalMidiInChannel (3);
            expectEquals (TestUtils::rms (f.render (noteOn (60, 5), 2)), 0.0f, "Without MPE the global channel filters notes");
            f.synth.allNotesOff (0, false);
            f.synth.setMpeSettings (true, 48);
            expectGreaterThan (TestUtils::rms (f.render (noteOn (60, 5), 2)), 0.01f);
            f.synth.allNotesOff (0, false);
            sound->setParameterByNameInt (SourceIDs::midiChannel, 2);
            expectGreaterThan (TestUtils::rms (f.render (noteOn (60, 7), 2)), 0.01f);
        }

        // Steady-state frequency measurement (number of positive zero crossings in the part after the note onset)
        auto steadyCrossings = [] (juce::AudioBuffer<float>& b)
        {
            juce::AudioBuffer<float> steady (b.getArrayOfWritePointers(), 1, 4 * blockSize, b.getNumSamples() - 4 * blockSize);
            return TestUtils::countPositiveZeroCrossings (steady);
        };
        auto pitchBend = [] (int channel, int value)
        {
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::pitchWheel (channel, value), 0);
            return midi;
        };

        beginTest ("MPE: per-note pitch bend (glide) uses the MPE bend range, also when sent before the note-on");
        {
            Fixture f;
            auto* sound = f.addSound (f.sineFile, 60);
            expect (TestUtils::runMessageLoopUntil ([&] { return f.synth.getNumSounds() == 1; }));
            sound->setParameterByNameFloat (SourceIDs::gain, 0.0f, false);
            f.synth.setMpeSettings (true, 48);
            auto reference = f.render (noteOn (60, 2), 24);
            const int referenceCrossings = steadyCrossings (reference);
            f.synth.allNotesOff (0, false);

            // +12 semitones with a 48 semitone range, sent before the note-on (as MPE controllers do)
            f.render (pitchBend (3, 8192 + 2048), 1);
            auto bent = f.render (noteOn (60, 3), 24);
            expectWithinAbsoluteError (steadyCrossings (bent), referenceCrossings * 2, 4);
            f.synth.allNotesOff (0, false);

            // A bend on another member channel does not affect this note
            f.render (pitchBend (4, 8192), 1);
            f.render (noteOn (60, 4), 1);
            f.render (pitchBend (5, 8192 + 2048), 1);
            auto unaffected = f.render ({}, 24);
            expectWithinAbsoluteError (steadyCrossings (unaffected), referenceCrossings, 3);
        }

        beginTest ("MPE: master channel pitch bend affects all notes with the sound's bend range");
        {
            Fixture f;
            auto* sound = f.addSound (f.sineFile, 60);
            expect (TestUtils::runMessageLoopUntil ([&] { return f.synth.getNumSounds() == 1; }));
            sound->setParameterByNameFloat (SourceIDs::pitchBendRangeUp, 12.0f, false);
            f.synth.setMpeSettings (true, 48);
            auto reference = f.render (noteOn (60, 2), 24);
            const int referenceCrossings = steadyCrossings (reference);
            f.synth.allNotesOff (0, false);
            f.render (noteOn (60, 6), 1);
            f.render (pitchBend (MPE_MASTER_CHANNEL, 16383), 1);  // Full bend up = +12 semitones
            auto bent = f.render ({}, 24);
            expectWithinAbsoluteError (steadyCrossings (bent), referenceCrossings * 2, 4);
        }

        beginTest ("MPE: mappings modulate parameters per note (slide -> gain), only when MPE is enabled");
        {
            Fixture f;
            auto* sound = f.addSound (f.sineFile, 60);
            expect (TestUtils::runMessageLoopUntil ([&] { return f.synth.getNumSounds() == 1; }));
            // Slide 0 -> gain -80 dB, slide 1 -> gain +12 dB (normalised range 0..1 of the gain parameter)
            sound->addOrEditMpeMapping (juce::Uuid().toString(), MPE_SOURCE_SLIDE, "gain", 0.0f, 1.0f, 0.0f);
            auto slide = [] (int channel, int value)
            {
                juce::MidiBuffer midi;
                midi.addEvent (juce::MidiMessage::controllerEvent (channel, MPE_SLIDE_CC, value), 0);
                return midi;
            };

            f.synth.setMpeSettings (false, 48);
            f.render (slide (2, 0), 1);
            const float withoutMpe = TestUtils::rms (f.render (noteOn (60, 2), 8));
            expectGreaterThan (withoutMpe, 0.01f, "Mappings must be ignored when MPE is disabled");
            f.synth.allNotesOff (0, false);

            f.synth.setMpeSettings (true, 48);
            f.render (slide (2, 0), 1);
            const float silent = TestUtils::rms (f.render (noteOn (60, 2), 8));
            f.synth.allNotesOff (0, false);
            f.render (slide (3, 127), 1);
            const float loud = TestUtils::rms (f.render (noteOn (60, 3), 8));
            expectLessThan (silent, 0.001f);
            expectGreaterThan (loud, withoutMpe);
            expectEquals ((float) sound->state.getProperty (SourceIDs::gain), SourceDefaults::gain, "The sound parameter itself must not change");

            // Changing the slide while the note plays changes its level
            f.render (slide (3, 0), 2);
            expectLessThan (TestUtils::rms (f.render ({}, 4)), 0.001f);

            // Each voice publishes the value of each mapping (for the UI)
            SourceSamplerVoice* voice = nullptr;
            for (int i = 0; i < f.synth.getNumVoices(); ++i)
                if (f.synth.getVoice (i)->isVoiceActive())
                    voice = static_cast<SourceSamplerVoice*> (f.synth.getVoice (i));
            expect (voice != nullptr);
            if (voice != nullptr)
            {
                expectEquals (voice->getPublishedNumMpeValues(), 1);
                expectWithinAbsoluteError (voice->getPublishedMpeValue (0), 0.0f, 1.0e-4f);
                f.render (slide (3, 127), 2);
                expectWithinAbsoluteError (voice->getPublishedMpeValue (0), 1.0f, 1.0e-4f);
            }
        }

        beginTest ("Modulators: state, validation and removal");
        {
            Fixture f;
            auto* sound = f.addSound (f.sineFile, 60);
            const auto uuid = juce::Uuid().toString();
            sound->addOrEditModulator (uuid, MODULATOR_TYPE_LFO, "gain", 0.0f, 0.5f, SourceHelpers::lfoTriangle, 3.0f, 0.1f, 0.2f, 0.3f, 0.4f);
            auto modulator = sound->state.getChildWithProperty (SourceIDs::uuid, uuid);
            expect (modulator.hasType (SourceIDs::MODULATOR));
            expectEquals (modulator.getProperty (SourceIDs::parameterName).toString(), juce::String ("gain"));
            expectEquals ((float) modulator.getProperty (SourceIDs::lfoRate), 3.0f);
            sound->addOrEditModulator (uuid, MODULATOR_TYPE_ADSR, "pitch", -2.0f, 1.0f, 0, 1.0f, 0.1f, 0.2f, 0.3f, 0.4f);
            expectEquals (modulator.getProperty (SourceIDs::modulatorType).toString(), juce::String (MODULATOR_TYPE_ADSR));
            expectEquals ((float) modulator.getProperty (SourceIDs::minRange), -1.0f, "Ranges are clamped to -1..1");
            sound->addOrEditModulator (juce::Uuid().toString(), MODULATOR_TYPE_LFO, "launchMode", 0.0f, 1.0f, 0, 1.0f, 0.1f, 0.2f, 0.3f, 0.4f);
            sound->addOrEditModulator (juce::Uuid().toString(), "wobble", "gain", 0.0f, 1.0f, 0, 1.0f, 0.1f, 0.2f, 0.3f, 0.4f);
            int numModulators = 0;
            for (const auto& child : sound->state)
                numModulators += child.hasType (SourceIDs::MODULATOR) ? 1 : 0;
            expectEquals (numModulators, 1, "Integer parameters and unknown types are rejected");
            expect (sound->hasModulators());
            sound->removeModulator (uuid);
            expect (! sound->hasModulators());
            expect (! sound->state.getChildWithProperty (SourceIDs::uuid, uuid).isValid());
        }

        beginTest ("Sounds remember the last value of each CC received on their channel (for the UI)");
        {
            Fixture f;
            auto* sound = f.addSound (f.sineFile, 60);
            expectEquals (sound->getLastCcValue (74), -1.0f, "Nothing received yet");
            sound->applyMidiCCModulations (1, 74, 127);
            expectEquals (sound->getLastCcValue (74), 1.0f);
            expectEquals (sound->getLastCcValue (1), -1.0f);
            expectEquals (sound->getLastCcValue (200), -1.0f);
        }

        beginTest ("Old Mod/Vel parameters are converted to aftertouch and velocity modulators");
        {
            auto soundState = SourceHelpers::createEmptySourceSoundState();
            soundState.setProperty (SourceIDs::filterCutoff, 1000.0f, nullptr);
            soundState.setProperty ("mod2GainAmt", 6.0f, nullptr);
            soundState.setProperty ("mod2CutoffAmt", 10.0f, nullptr);
            soundState.setProperty ("mod2PitchAmt", 0.0f, nullptr);
            soundState.setProperty ("mod2PlayheadPos", 0.0f, nullptr);
            soundState.setProperty ("vel2GainAmt", 0.5f, nullptr);
            soundState.setProperty ("vel2CutoffAmt", 0.0f, nullptr);
            SourceSound::convertLegacyModulationParameters (soundState);
            std::vector<juce::ValueTree> modulators;
            for (const auto& child : soundState)
                if (child.hasType (SourceIDs::MODULATOR))
                    modulators.push_back (child);
            expectEquals ((int) modulators.size(), 3, "Only parameters that were not 0");
            auto find = [&modulators] (const juce::String& type, const juce::String& parameter)
            {
                for (const auto& m : modulators)
                    if (m.getProperty (SourceIDs::modulatorType).toString() == type && m.getProperty (SourceIDs::parameterName).toString() == parameter)
                        return m;
                return juce::ValueTree();
            };
            auto gain = find (MODULATOR_TYPE_AFTERTOUCH, "gain");
            expect (gain.isValid());
            expectWithinAbsoluteError ((float) gain.getProperty (SourceIDs::maxRange), 6.0f / 92.0f, 1.0e-4f);
            auto cutoff = find (MODULATOR_TYPE_AFTERTOUCH, "filterCutoff");
            expect (cutoff.isValid());
            expectWithinAbsoluteError ((float) cutoff.getProperty (SourceIDs::maxRange), 10000.0f / 19990.0f, 1.0e-4f, "Cutoff x11 at full aftertouch");
            auto velocity = find (MODULATOR_TYPE_VELOCITY, "gain");
            expect (velocity.isValid());
            expectWithinAbsoluteError ((float) velocity.getProperty (SourceIDs::minRange), juce::Decibels::gainToDecibels (0.5f) / 92.0f, 1.0e-3f);
            expectEquals ((float) velocity.getProperty (SourceIDs::maxRange), 0.0f);
            expect (! soundState.hasProperty ("mod2GainAmt") && ! soundState.hasProperty ("vel2GainAmt"), "Old properties removed");
            SourceSound::convertLegacyModulationParameters (soundState);
            int numAfter = 0;
            for (const auto& child : soundState)
                numAfter += child.hasType (SourceIDs::MODULATOR) ? 1 : 0;
            expectEquals (numAfter, 3, "Converting again does nothing");
        }

        beginTest ("Velocity and aftertouch modulators, and muted sounds");
        {
            Fixture f;
            auto* sound = f.addSound (f.sineFile, 60);
            expect (TestUtils::runMessageLoopUntil ([&] { return f.synth.getNumSounds() == 1; }));
            auto noteWithVelocity = [] (juce::uint8 velocity)
            {
                juce::MidiBuffer midi;
                midi.addEvent (juce::MidiMessage::noteOn (1, 60, velocity), 0);
                return midi;
            };
            // No velocity modulator: velocity doesn't change the level
            const float loudNoMod = TestUtils::rms (f.render (noteWithVelocity (127), 4));
            f.synth.allNotesOff (0, false);
            const float softNoMod = TestUtils::rms (f.render (noteWithVelocity (20), 4));
            f.synth.allNotesOff (0, false);
            expectWithinAbsoluteError (softNoMod, loudNoMod, loudNoMod * 0.02f);

            const auto velocityUuid = juce::Uuid().toString();
            sound->addOrEditModulator (velocityUuid, MODULATOR_TYPE_VELOCITY, "gain", -0.5f, 0.0f, 0, 1.0f, 0.1f, 0.1f, 1.0f, 0.1f);
            const float loud = TestUtils::rms (f.render (noteWithVelocity (127), 4));
            f.synth.allNotesOff (0, false);
            const float soft = TestUtils::rms (f.render (noteWithVelocity (20), 4));
            f.synth.allNotesOff (0, false);
            expectWithinAbsoluteError (loud, loudNoMod, loudNoMod * 0.02f, "Full velocity: max (0) offset");
            expectLessThan (soft, loud * 0.1f, "Low velocity: towards the min offset");
            sound->removeModulator (velocityUuid);

            // Aftertouch (channel pressure) moves the gain from min (silent) to max (normal)
            sound->addOrEditModulator (juce::Uuid().toString(), MODULATOR_TYPE_AFTERTOUCH, "gain", -1.0f, 0.0f, 0, 1.0f, 0.1f, 0.1f, 1.0f, 0.1f);
            expectLessThan (TestUtils::rms (f.render (noteOn (60), 4)), 0.001f, "No pressure");
            juce::MidiBuffer pressure;
            pressure.addEvent (juce::MidiMessage::channelPressureChange (1, 127), 0);
            f.render (pressure, 2);
            expectGreaterThan (TestUtils::rms (f.render ({}, 4)), loudNoMod * 0.8f, "Full pressure");
            f.synth.allNotesOff (0, false);

            // Muted sounds are silent (and unmuted again)
            sound->setMuted (true);
            expect ((bool) sound->state.getProperty (SourceIDs::muted));
            juce::MidiBuffer full;
            full.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 127), 0);
            full.addEvent (juce::MidiMessage::channelPressureChange (1, 127), 1);
            expectLessThan (TestUtils::rms (f.render (full, 4)), 1.0e-6f);
            sound->setMuted (false);
            expectGreaterThan (TestUtils::rms (f.render ({}, 4)), loudNoMod * 0.8f, "The voice kept playing while muted");
        }

        beginTest ("Manual slices: n markers make n + 1 slices, each note plays its slice");
        {
            Fixture f;
            auto* sound = f.addSound (f.sineFile, 60);  // 2 s, all notes mapped
            expect (TestUtils::runMessageLoopUntil ([&] { return f.synth.getNumSounds() == 1; }));
            expect (sound->getManualSlices().empty(), "New sounds have no manual slices");
            sound->setParameterByNameInt (SourceIDs::noteMappingMode, NOTE_MAPPING_MODE_SLICE);
            sound->setParameterByNameInt (SourceIDs::numSlices, SLICE_MODE_MANUAL);
            sound->setManualSlices ({ 1.0f, 0.5f });  // Unsorted on purpose
            expectEquals (sound->state.getProperty (SourceIDs::slices).toString(), juce::String ("0.5000,1.0000"));
            auto playPositionOf = [&f] (int note)
            {
                f.synth.allNotesOff (0, false);
                f.render (noteOn (note), 1);
                for (int i = 0; i < f.synth.getNumVoices(); ++i)
                    if (auto* voice = static_cast<SourceSamplerVoice*> (f.synth.getVoice (i)); voice->isVoiceActive())
                        return voice->getPublishedState().playPosition;
                return -1.0f;
            };
            // Note indices: all 128 notes are mapped, so note n has index n (3 slices: n % 3)
            expectWithinAbsoluteError (playPositionOf (60), 0.0f, 0.02f);
            expectWithinAbsoluteError (playPositionOf (61), 0.25f, 0.02f);
            expectWithinAbsoluteError (playPositionOf (62), 0.5f, 0.02f);
            expectWithinAbsoluteError (playPositionOf (63), 0.0f, 0.02f);

            // Equal slices ignore the markers
            sound->setParameterByNameInt (SourceIDs::numSlices, 4);
            expectWithinAbsoluteError (playPositionOf (61), 0.25f, 0.02f);
            expectWithinAbsoluteError (playPositionOf (63), 0.75f, 0.02f);
            expectEquals (sound->getManualSlices().size(), (size_t) 2, "Manual slices are kept");

            // Slices from the Freesound onsets (none for this local sound)
            expect (sound->getOnsetTimes().empty());
            sound->setManualSlices ({});
            expect (sound->getManualSlices().empty());
        }

        beginTest ("Slices of presets from older versions are converted; slices from onsets");
        {
            Fixture f;
            juce::BigInteger allNotes;
            allNotes.setRange (0, 8, true);
            // numSlices 0 meant "slices from the onsets": the onsets become the manual slices
            auto onsetsSound = SourceHelpers::createSourceSoundAndSourceSamplerSoundFromProperties (
                -1, "a", "", "", "", f.sineFile.getFullPathName(), "wav", 1, { "0.001", "0.5", "1.0" }, allNotes, 60, 0, 0);
            onsetsSound.removeProperty (SourceIDs::slices, nullptr);
            onsetsSound.setProperty (SourceIDs::numSlices, 0, nullptr);
            // numSlices 1 meant "one slice per mapped note": now that number of equal slices
            auto notesSound = SourceHelpers::createSourceSoundAndSourceSamplerSoundFromProperties (
                -1, "b", "", "", "", f.sineFile.getFullPathName(), "wav", 1, {}, allNotes, 60, 0, 0);
            notesSound.removeProperty (SourceIDs::slices, nullptr);
            notesSound.setProperty (SourceIDs::numSlices, 1, nullptr);
            f.preset.addChild (onsetsSound, -1, nullptr);
            f.preset.addChild (notesSound, -1, nullptr);
            auto* converted = f.sounds->getSoundWithUUID (onsetsSound.getProperty (SourceIDs::uuid).toString());
            expect (converted != nullptr);
            if (converted != nullptr)
            {
                expectEquals (onsetsSound.getProperty (SourceIDs::slices).toString(), juce::String ("0.5000,1.0000"), "Onset at the start dropped");
                expectEquals ((int) converted->getOnsetTimes().size(), 3);
                converted->setManualSlices ({});
                converted->setSlicesFromOnsets();
                expectEquals ((int) converted->getManualSlices().size(), 2);
            }
            expectEquals ((int) notesSound.getProperty (SourceIDs::numSlices), 8);
            // Sounds that already have the property are not converted again
            auto newSound = SourceHelpers::createSourceSoundAndSourceSamplerSoundFromProperties (
                -1, "c", "", "", "", f.sineFile.getFullPathName(), "wav", 1, { "0.5" }, allNotes, 60, 0, 0);
            f.preset.addChild (newSound, -1, nullptr);
            expectEquals (newSound.getProperty (SourceIDs::slices).toString(), juce::String(), "New sounds start without manual slices");
            expectEquals (SourceHelpers::cleanSliceTimes ({ 0.3f, -1.0f, 0.001f, 0.3f, 0.2f }).size(), (size_t) 2);
        }

        beginTest ("Modulators, MPE mappings and MIDI CC mappings share a limit of 16 per sound");
        {
            Fixture f;
            auto* sound = f.addSound (f.sineFile, 60);
            for (int i = 0; i < 10; ++i)
                sound->addOrEditModulator (juce::Uuid().toString(), MODULATOR_TYPE_LFO, "gain", 0.0f, 0.0f, 0, 1.0f, 0.1f, 0.1f, 1.0f, 0.1f);
            for (int i = 0; i < 4; ++i)
                sound->addOrEditMidiMapping (juce::Uuid().toString(), 10 + i, "gain", 0.0f, 1.0f);
            for (int i = 0; i < 2; ++i)
                sound->addOrEditMpeMapping (juce::Uuid().toString(), MPE_SOURCE_SLIDE, "gain", 0.0f, 1.0f, 0.0f);
            expectEquals (sound->getNumModulations(), 16);
            sound->addOrEditModulator (juce::Uuid().toString(), MODULATOR_TYPE_ADSR, "gain", 0.0f, 0.0f, 0, 1.0f, 0.1f, 0.1f, 1.0f, 0.1f);
            sound->addOrEditMidiMapping (juce::Uuid().toString(), 20, "gain", 0.0f, 1.0f);
            sound->addOrEditMpeMapping (juce::Uuid().toString(), MPE_SOURCE_GLIDE, "gain", 0.0f, 1.0f, 0.0f);
            expectEquals (sound->getNumModulations(), 16, "No more can be added");
        }

        beginTest ("Modulators: a square LFO on gain modulates the level of each note, the default range does nothing");
        {
            Fixture f;
            auto* sound = f.addSound (f.sineFile, 60);
            expect (TestUtils::runMessageLoopUntil ([&] { return f.synth.getNumSounds() == 1; }));
            const float reference = TestUtils::rms (f.render (noteOn (60), 8));
            f.synth.allNotesOff (0, false);

            const auto uuid = juce::Uuid().toString();
            sound->addOrEditModulator (uuid, MODULATOR_TYPE_LFO, "gain", 0.0f, 0.0f, SourceHelpers::lfoSquare, 2.0f, 0.1f, 0.1f, 1.0f, 0.1f);
            const float withZeroRange = TestUtils::rms (f.render (noteOn (60), 8));
            expectWithinAbsoluteError (withZeroRange, reference, reference * 0.02f, "min = max = 0 means no modulation");
            f.synth.allNotesOff (0, false);

            // Square at 2 Hz: the first 0.25 s the signal is 1 (gain -1 normalised -> -80 dB), the next 0.25 s it is 0
            sound->addOrEditModulator (uuid, MODULATOR_TYPE_LFO, "gain", 0.0f, -1.0f, SourceHelpers::lfoSquare, 2.0f, 0.1f, 0.1f, 1.0f, 0.1f);
            auto output = f.render (noteOn (60), 43);  // ~0.5 s
            auto segment = [&output] (double start, double end)
            {
                const int first = (int) (start * sampleRate), num = (int) ((end - start) * sampleRate);
                juce::AudioBuffer<float> part (2, num);
                for (int c = 0; c < 2; ++c)
                    part.copyFrom (c, 0, output, c, first, num);
                return TestUtils::rms (part);
            };
            expectLessThan (segment (0.02, 0.23), 0.001f, "Silent while the LFO is high");
            expectGreaterThan (segment (0.27, 0.48), reference * 0.8f, "Normal level while the LFO is low");
            expectEquals ((float) sound->state.getProperty (SourceIDs::gain), SourceDefaults::gain, "The sound parameter itself must not change");

            // The voice publishes the modulator output for the UI
            SourceSamplerVoice* voice = nullptr;
            for (int i = 0; i < f.synth.getNumVoices(); ++i)
                if (f.synth.getVoice (i)->isVoiceActive())
                    voice = static_cast<SourceSamplerVoice*> (f.synth.getVoice (i));
            expect (voice != nullptr);
            if (voice == nullptr)
                return;
            const auto count = voice->getModulatorHistoryCount();
            expectGreaterThan ((int) count, 90, "About 200 points per second");
            float minSeen = 1.0f, maxSeen = -1.0f;
            for (uint32_t i = count - 90; i < count; ++i)
            {
                minSeen = juce::jmin (minSeen, voice->getModulatorHistoryValue (0, i));
                maxSeen = juce::jmax (maxSeen, voice->getModulatorHistoryValue (0, i));
            }
            expectEquals (minSeen, -1.0f);
            expectEquals (maxSeen, 0.0f);
        }

        beginTest ("Modulators: synced LFOs follow the tempo, and the LFO attack delays the modulation");
        {
            Fixture f;
            auto* sound = f.addSound (f.sineFile, 60);
            expect (TestUtils::runMessageLoopUntil ([&] { return f.synth.getNumSounds() == 1; }));
            const float reference = TestUtils::rms (f.render (noteOn (60), 8));
            f.synth.allNotesOff (0, false);
            auto segment = [] (const juce::AudioBuffer<float>& output, double start, double end)
            {
                const int first = (int) (start * sampleRate), num = (int) ((end - start) * sampleRate);
                juce::AudioBuffer<float> part (2, num);
                for (int c = 0; c < 2; ++c)
                    part.copyFrom (c, 0, output, c, first, num);
                return TestUtils::rms (part);
            };
            const int quarterDivision = SourceDefaults::lfoSyncDivision;  // "1/4"
            const auto uuid = juce::Uuid().toString();

            // Synced square, one cycle per beat: at 120 BPM the first 0.25 s are silent (signal 1 -> gain -1), then 0.25 s loud
            sound->addOrEditModulator (uuid, MODULATOR_TYPE_LFO, "gain", 0.0f, -1.0f, SourceHelpers::lfoSquare, 10.0f, 0.1f, 0.1f, 1.0f, 0.1f,
                                       0.0f, true, quarterDivision, 0);
            f.synth.setCurrentBpm (120.0);
            auto fast = f.render (noteOn (60), 43);
            f.synth.allNotesOff (0, false);
            expectLessThan (segment (fast, 0.02, 0.23), 0.001f);
            expectGreaterThan (segment (fast, 0.27, 0.48), reference * 0.8f);

            // At 60 BPM the first half cycle lasts 0.5 s
            f.synth.setCurrentBpm (60.0);
            auto slow = f.render (noteOn (60), 43);
            f.synth.allNotesOff (0, false);
            expectLessThan (segment (slow, 0.02, 0.48), 0.001f);

            // Free LFO at 2 Hz with a 1 s attack: the first half cycle takes much longer than 0.25 s
            sound->addOrEditModulator (uuid, MODULATOR_TYPE_LFO, "gain", 0.0f, -1.0f, SourceHelpers::lfoSquare, 2.0f, 0.1f, 0.1f, 1.0f, 0.1f,
                                       1.0f, false, quarterDivision, 0);
            auto withAttack = f.render (noteOn (60), 43);
            f.synth.allNotesOff (0, false);
            expectLessThan (segment (withAttack, 0.27, 0.48), 0.001f, "Still in the first half cycle");

            // Synced attack ("1/4" = index 9 in the attack choices) at 120 BPM: 0.5 s
            sound->addOrEditModulator (uuid, MODULATOR_TYPE_LFO, "gain", 0.0f, -1.0f, SourceHelpers::lfoSquare, 2.0f, 0.1f, 0.1f, 1.0f, 0.1f,
                                       0.0f, true, quarterDivision, quarterDivision + 1);
            f.synth.setCurrentBpm (120.0);
            auto syncedAttack = f.render (noteOn (60), 43);
            expectLessThan (segment (syncedAttack, 0.27, 0.48), 0.001f, "The synced attack also delays the LFO");
        }

        beginTest ("Modulators: steps are cycled at their rate or, synced, over the cycle length");
        {
            Fixture f;
            auto* sound = f.addSound (f.sineFile, 60);
            expect (TestUtils::runMessageLoopUntil ([&] { return f.synth.getNumSounds() == 1; }));
            const float reference = TestUtils::rms (f.render (noteOn (60), 8));
            f.synth.allNotesOff (0, false);
            auto segment = [] (const juce::AudioBuffer<float>& output, double start, double end)
            {
                const int first = (int) (start * sampleRate), num = (int) ((end - start) * sampleRate);
                juce::AudioBuffer<float> part (2, num);
                for (int c = 0; c < 2; ++c)
                    part.copyFrom (c, 0, output, c, first, num);
                return TestUtils::rms (part);
            };
            // Two steps (silent, normal) on gain: signal 1 -> -1 (silent), signal 0 -> no change
            const auto uuid = juce::Uuid().toString();
            sound->addOrEditModulator (uuid, MODULATOR_TYPE_STEPS, "gain", 0.0f, -1.0f, 0, 1.0f, 0.1f, 0.1f, 1.0f, 0.1f,
                                       0.0f, false, 0, 0, 2, 2.0f, 0, "1,0");
            auto free = f.render (noteOn (60), 43);  // 2 cycles per second: 0.25 s per step
            f.synth.allNotesOff (0, false);
            expectLessThan (segment (free, 0.02, 0.23), 0.001f);
            expectGreaterThan (segment (free, 0.27, 0.48), reference * 0.8f);
            expectEquals (sound->state.getChildWithProperty (SourceIDs::uuid, uuid).getProperty (SourceIDs::numSteps).toString(), juce::String ("2"));

            // Synced, cycle of "1 beat" at 60 BPM: 0.5 s per step
            sound->addOrEditModulator (uuid, MODULATOR_TYPE_STEPS, "gain", 0.0f, -1.0f, 0, 1.0f, 0.1f, 0.1f, 1.0f, 0.1f,
                                       0.0f, true, 0, 0, 2, 2.0f, 1, "1,0");
            f.synth.setCurrentBpm (60.0);
            auto synced = f.render (noteOn (60), 43);
            expectEquals ((int) sound->state.getChildWithProperty (SourceIDs::uuid, uuid).getProperty (SourceIDs::stepsSyncLength), 1, "Editing updates the steps settings");
            expectLessThan (segment (synced, 0.02, 0.48), 0.001f);
        }

        beginTest ("Modulators: an ADSR fades each note in and is released on note off");
        {
            Fixture f;
            auto* sound = f.addSound (f.sineFile, 60);
            expect (TestUtils::runMessageLoopUntil ([&] { return f.synth.getNumSounds() == 1; }));
            sound->setParameterByNameFloat (SourceIDs::release, 1.0f, false);  // Long amp release, so the note keeps sounding
            // Envelope 0 -> gain -1 (silent), envelope 1 -> no change
            sound->addOrEditModulator (juce::Uuid().toString(), MODULATOR_TYPE_ADSR, "gain", -1.0f, 0.0f, 0, 1.0f, 0.3f, 0.0f, 1.0f, 0.05f);
            const float start = TestUtils::rms (f.render (noteOn (60), 2));
            f.render ({}, 30);
            const float sustained = TestUtils::rms (f.render ({}, 4));
            expectLessThan (start, sustained * 0.3f);
            f.render (noteOff (60), 1);
            f.render ({}, 8);
            expectLessThan (TestUtils::rms (f.render ({}, 4)), sustained * 0.05f, "The modulator envelope was released");
        }

        beginTest ("Corrupted files are not loaded");
        {
            Fixture f;
            auto badFile = f.tmp.folder.getChildFile ("bad.wav");
            badFile.replaceWithText (juce::String::repeatedString ("not audio ", 100));
            auto* sound = f.addSound (badFile, 60);
            expect (sound != nullptr);
            TestUtils::runMessageLoopFor (500);
            expectEquals (f.synth.getNumSounds(), 0);
            expect (badFile.existsAsFile(), "User files must never be deleted");
        }
    }
};

static SamplerRenderTests samplerRenderTests;
