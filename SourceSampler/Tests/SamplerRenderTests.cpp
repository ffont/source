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
            sound->setParameterByNameFloat (SourceIDs::vel2GainAmt, 0.0f, false);
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
