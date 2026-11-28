/*
  ==============================================================================

    SourceSamplerTests.cpp - integration tests of SourceSampler through its
    action-message API (the API used by all UIs). Uses a temporary data folder
    so the user's presets/settings are never touched. No network access needed.

  ==============================================================================
*/

#include <JuceHeader.h>
#include "TestUtils.h"
#include "SourceSampler.h"
#include <thread>


class SourceSamplerTests : public juce::UnitTest
{
public:
    SourceSamplerTests() : juce::UnitTest ("SourceSampler", "Source") {}

    static constexpr double sampleRate = 44100.0;
    static constexpr int blockSize = 512;

    struct Fixture
    {
        Fixture() : sampler (tmp.folder)
        {
            sampler.prepareToPlay (sampleRate, blockSize);
        }

        void send (const juce::String& action, const juce::StringArray& parameters = {})
        {
            sampler.actionListenerCallback (SourceHelpers::serializeAction (action, parameters));
        }

        juce::ValueTree preset() { return sampler.state.getChildWithName (SourceIDs::PRESET); }

        int numSounds()
        {
            int n = 0;
            for (const auto& child : preset())
                if (child.hasType (SourceIDs::SOUND)) ++n;
            return n;
        }

        juce::ValueTree addLocalSound (const juce::String& name, double frequency)
        {
            auto file = TestUtils::writeSineWav (tmp.folder.getChildFile (name + ".wav"), frequency, 0.5, sampleRate);
            const int before = numSounds();
            send (ACTION_ADD_SOUND, { "-1", name, "", "", "", file.getFullPathName(), "wav", juce::String (file.getSize()), "", "", "60" });
            auto sound = preset().getChild (before);
            TestUtils::runMessageLoopUntil ([sound] { return (bool) sound.getProperty (SourceIDs::allSoundsLoaded); });
            return sound;
        }

        juce::AudioBuffer<float> process (juce::MidiBuffer midi = {}, int numBlocks = 1)
        {
            juce::AudioBuffer<float> output (2, blockSize * numBlocks);
            output.clear();
            for (int b = 0; b < numBlocks; ++b)
            {
                juce::AudioBuffer<float> block (output.getArrayOfWritePointers(), 2, b * blockSize, blockSize);
                juce::MidiBuffer blockMidi;
                if (b == 0)
                    blockMidi = midi;
                sampler.processBlock (block, blockMidi);
            }
            return output;
        }

        TestUtils::TemporaryFolder tmp;  // Declared first so it outlives the sampler
        SourceSampler sampler;
    };

    void runTest() override
    {
        beginTest ("Data folders are created in the given base location");
        {
            Fixture f;
            expect (f.tmp.folder.getChildFile (SOURCE_APP_DIRECTORY_NAME).isDirectory());
            expect (f.sampler.presetFilesLocation.isAChildOf (f.tmp.folder));
            expect (f.preset().isValid());
        }

        beginTest ("Polyphony is applied and clamped");
        {
            Fixture f;
            f.send (ACTION_SET_POLYPHONY, { "4" });
            expectEquals ((int) f.sampler.getVoicesInfo().size(), 4);
            expectEquals ((int) f.preset().getProperty (SourceIDs::numVoices), 4);
            f.send (ACTION_SET_POLYPHONY, { "1000" });
            expectEquals ((int) f.sampler.getVoicesInfo().size(), (int) SourceSamplerSynthesiser::maxNumVoices);
            f.send (ACTION_SET_POLYPHONY, { "0" });
            expectEquals ((int) f.sampler.getVoicesInfo().size(), 1);
        }

        beginTest ("Local sounds can be added, played from the UI and removed");
        {
            Fixture f;
            auto sound = f.addLocalSound ("sine", 440.0);
            expect ((bool) sound.getProperty (SourceIDs::allSoundsLoaded), "Sound was not loaded");
            const auto uuid = sound.getProperty (SourceIDs::uuid).toString();

            f.send (ACTION_PLAY_SOUND, { uuid });
            expectGreaterThan (TestUtils::rms (f.process ({}, 4)), 0.01f);
            const auto levels = f.sampler.getOutputLevels();
            expectEquals ((int) levels.size(), 2);
            expectGreaterThan (levels[0], 0.01f, "Output level meter should measure the playing sound");
            auto voices = f.sampler.getVoicesInfo();
            expect (std::any_of (voices.begin(), voices.end(), [&] (const auto& v) { return v.active && v.soundUUID == uuid; }));

            f.send (ACTION_REMOVE_SOUND, { uuid });
            expect (TestUtils::runMessageLoopUntil ([&] { return f.numSounds() == 0; }, 2000), "Sound was not removed");
            expectEquals (TestUtils::rms (f.process ({}, 2)), 0.0f);
        }

        beginTest ("Note layouts are re-applied to all sounds");
        {
            Fixture f;
            f.addLocalSound ("a", 440.0);
            f.addLocalSound ("b", 660.0);
            f.send (ACTION_REAPPLY_LAYOUT, { juce::String (NOTE_MAPPING_TYPE_CONTIGUOUS) });
            for (int i = 0; i < 2; ++i)
            {
                juce::BigInteger notes;
                notes.parseString (f.preset().getChild (i).getProperty (SourceIDs::midiNotes).toString(), 16);
                expect (notes == SourceHelpers::computeNoteLayout (NOTE_MAPPING_TYPE_CONTIGUOUS, i, 2).midiNotes);
            }
            f.send (ACTION_REAPPLY_LAYOUT, { juce::String (NOTE_MAPPING_TYPE_ALL) });
            expectEquals ((int) f.preset().getChild (1).getProperty (SourceIDs::midiChannel), 2);
        }

        beginTest ("Sound parameters can be set for one or all sounds, unknown parameters are ignored");
        {
            Fixture f;
            auto a = f.addLocalSound ("a", 440.0);
            auto b = f.addLocalSound ("b", 660.0);
            f.send (ACTION_SET_SOUND_PARAMETER_FLOAT, { a.getProperty (SourceIDs::uuid).toString(), "gain", "-3.5" });
            expectEquals ((float) a.getProperty (SourceIDs::gain), -3.5f);
            expectEquals ((float) b.getProperty (SourceIDs::gain), SourceDefaults::gain);
            f.send (ACTION_SET_SOUND_PARAMETER_FLOAT, { "", "pan", "0.25" });
            expectEquals ((float) a.getProperty (SourceIDs::pan), 0.25f);
            expectEquals ((float) b.getProperty (SourceIDs::pan), 0.25f);
            f.send (ACTION_SET_SOUND_PARAMETER_FLOAT, { "", "gain", "1000" });  // Out of range values are clamped
            expectEquals ((float) a.getProperty (SourceIDs::gain), 12.0f);
            f.send (ACTION_SET_SOUND_PARAMETER_INT, { "", "launchMode", "3" });
            expectEquals ((int) b.getProperty (SourceIDs::launchMode), 3);
           #if ! JUCE_DEBUG  // Unknown names hit a jassert in debug builds
            f.send (ACTION_SET_SOUND_PARAMETER_FLOAT, { "", "notAParameter", "1" });
           #endif
        }

        beginTest ("Presets can be saved and loaded");
        {
            Fixture f;
            f.addLocalSound ("a", 440.0);
            f.send (ACTION_SET_REVERB_PARAMETERS, { "0.9", "0.1", "0.3", "0.7", "0.2", "0.0" });
            f.send (ACTION_SAVE_PRESET, { "My preset", "3" });
            expect (f.sampler.presetFilesLocation.getChildFile ("3.xml").existsAsFile());
            expectEquals (f.sampler.getPresetNameByIndex (3), juce::String ("My preset"));

            f.send (ACTION_LOAD_PRESET, { "5" });  // Non-existing preset creates an empty one
            expect (TestUtils::runMessageLoopUntil ([&] { return f.numSounds() == 0; }, 2000));
            expectEquals ((int) f.sampler.state.getProperty (SourceIDs::currentPresetIndex), 5);

            f.send (ACTION_LOAD_PRESET, { "3" });
            expectEquals (f.numSounds(), 1);
            expectEquals (f.preset().getProperty (SourceIDs::name).toString(), juce::String ("My preset"));
            expectEquals ((float) f.preset().getProperty (SourceIDs::reverbRoomSize), 0.9f);
            auto sound = f.preset().getChild (0);
            expect (TestUtils::runMessageLoopUntil ([sound] { return (bool) sound.getProperty (SourceIDs::allSoundsLoaded); }), "Preset sound not loaded");
        }

        beginTest ("Global settings persist between instances");
        {
            TestUtils::TemporaryFolder tmp;
            {
                SourceSampler sampler (tmp.folder);
                sampler.actionListenerCallback (SourceHelpers::serializeAction (ACTION_SET_GLOBAL_MIDI_IN_CHANNEL, { "7" }));
            }
            SourceSampler sampler (tmp.folder);
            expectEquals ((int) sampler.state.getProperty (SourceIDs::globalMidiInChannel), 7);
        }

        beginTest ("Global MIDI channel filters notes");
        {
            Fixture f;
            f.addLocalSound ("a", 440.0);
            f.send (ACTION_SET_GLOBAL_MIDI_IN_CHANNEL, { "3" });
            juce::MidiBuffer wrongChannel;
            wrongChannel.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 127), 0);
            expectEquals (TestUtils::rms (f.process (wrongChannel, 2)), 0.0f);
            juce::MidiBuffer rightChannel;
            rightChannel.addEvent (juce::MidiMessage::noteOn (3, 60, (juce::uint8) 127), 0);
            expectGreaterThan (TestUtils::rms (f.process (rightChannel, 2)), 0.01f);
            expectEquals (f.sampler.getLastReceivedMidiNote(), 60);
        }

        beginTest ("MIDI CC mappings received in the audio thread are applied in the message thread");
        {
            Fixture f;
            auto sound = f.addLocalSound ("a", 440.0);
            const auto uuid = sound.getProperty (SourceIDs::uuid).toString();
            f.send (ACTION_ADD_OR_UPDATE_CC_MAPPING, { uuid, juce::Uuid().toString(), "20", "pan", "0.0", "1.0" });
            expect (sound.getChildWithName (SourceIDs::MIDI_CC_MAPPING).isValid());

            juce::MidiBuffer cc;
            cc.addEvent (juce::MidiMessage::controllerEvent (1, 20, 127), 0);
            f.process (cc);
            expectEquals ((float) sound.getProperty (SourceIDs::pan), SourceDefaults::pan, "Should not be applied in the audio thread");
            expect (TestUtils::runMessageLoopUntil ([&] { return (float) sound.getProperty (SourceIDs::pan) == 1.0f; }, 1000), "CC mapping not applied");
        }

        beginTest ("MPE settings are applied and persisted");
        {
            TestUtils::TemporaryFolder tmp;
            {
                SourceSampler sampler (tmp.folder);
                sampler.actionListenerCallback (SourceHelpers::serializeAction (ACTION_SET_MPE_SETTINGS, { "1", "24" }));
                expect ((bool) sampler.state.getProperty (SourceIDs::mpeEnabled));
                expectEquals ((int) sampler.state.getProperty (SourceIDs::mpePitchBendRange), 24);
            }
            SourceSampler sampler (tmp.folder);
            expect ((bool) sampler.state.getProperty (SourceIDs::mpeEnabled));
            expectEquals ((int) sampler.state.getProperty (SourceIDs::mpePitchBendRange), 24);
            sampler.actionListenerCallback (SourceHelpers::serializeAction (ACTION_SET_MPE_SETTINGS, { "0", "500" }));
            expect (! (bool) sampler.state.getProperty (SourceIDs::mpeEnabled));
            expectEquals ((int) sampler.state.getProperty (SourceIDs::mpePitchBendRange), 96, "Bend range is clamped");
        }

        beginTest ("MPE mappings can be added, updated, removed, and are saved in presets");
        {
            Fixture f;
            auto sound = f.addLocalSound ("a", 440.0);
            const auto soundUUID = sound.getProperty (SourceIDs::uuid).toString();
            const auto mappingUUID = juce::Uuid().toString();
            f.send (ACTION_ADD_OR_UPDATE_MPE_MAPPING, { soundUUID, mappingUUID, MPE_SOURCE_STRIKE, "filterCutoff", "0.1", "0.9", "0.5" });
            auto mapping = sound.getChildWithProperty (SourceIDs::uuid, mappingUUID);
            expect (mapping.hasType (SourceIDs::MPE_MAPPING));
            expectEquals (mapping.getProperty (SourceIDs::mpeSource).toString(), juce::String (MPE_SOURCE_STRIKE));
            expectEquals ((float) mapping.getProperty (SourceIDs::curve), 0.5f);

            f.send (ACTION_ADD_OR_UPDATE_MPE_MAPPING, { soundUUID, mappingUUID, MPE_SOURCE_SLIDE, "pan", "0", "1", "5" });
            expectEquals (mapping.getProperty (SourceIDs::mpeSource).toString(), juce::String (MPE_SOURCE_SLIDE));
            expectEquals ((float) mapping.getProperty (SourceIDs::curve), 1.0f, "Curve is clamped");

            // Invalid sources / parameters (or int parameters) are ignored
            f.send (ACTION_ADD_OR_UPDATE_MPE_MAPPING, { soundUUID, juce::Uuid().toString(), "notASource", "pan", "0", "1", "0" });
            f.send (ACTION_ADD_OR_UPDATE_MPE_MAPPING, { soundUUID, juce::Uuid().toString(), "press", "pan", "0", "1", "0" });  // Pressure is used by aftertouch modulators
            f.send (ACTION_ADD_OR_UPDATE_MPE_MAPPING, { soundUUID, juce::Uuid().toString(), MPE_SOURCE_SLIDE, "notAParameter", "0", "1", "0" });
            f.send (ACTION_ADD_OR_UPDATE_MPE_MAPPING, { soundUUID, juce::Uuid().toString(), MPE_SOURCE_SLIDE, "launchMode", "0", "1", "0" });
            int numMappings = 0;
            for (const auto& child : sound)
                if (child.hasType (SourceIDs::MPE_MAPPING)) ++numMappings;
            expectEquals (numMappings, 1);

            f.send (ACTION_SAVE_PRESET, { "MPE preset", "4" });
            f.send (ACTION_LOAD_PRESET, { "6" });
            f.send (ACTION_LOAD_PRESET, { "4" });
            auto loadedSound = f.preset().getChild (0);
            auto loadedMapping = loadedSound.getChildWithProperty (SourceIDs::uuid, mappingUUID);
            expect (loadedMapping.hasType (SourceIDs::MPE_MAPPING), "MPE mapping not saved in preset");
            if (auto* loaded = f.sampler.getSoundWithUUID (loadedSound.getProperty (SourceIDs::uuid).toString()))
            {
                SourceSound::MpeMappings copy;
                expectEquals (loaded->copyMpeMappings (copy), 1, "Audio thread mappings not rebuilt after loading the preset");
                expect (copy[0].parameter == SourceIDs::pan);
            }

            f.send (ACTION_REMOVE_MPE_MAPPING, { loadedSound.getProperty (SourceIDs::uuid).toString(), mappingUUID });
            expect (! loadedSound.getChildWithProperty (SourceIDs::uuid, mappingUUID).isValid());
        }

        beginTest ("In MPE mode, controllers on member channels don't trigger MIDI CC mappings");
        {
            Fixture f;
            auto sound = f.addLocalSound ("a", 440.0);
            const auto uuid = sound.getProperty (SourceIDs::uuid).toString();
            f.send (ACTION_ADD_OR_UPDATE_CC_MAPPING, { uuid, juce::Uuid().toString(), "74", "pan", "0.0", "1.0" });
            f.send (ACTION_SET_MPE_SETTINGS, { "1", "48" });
            juce::MidiBuffer memberCC;
            memberCC.addEvent (juce::MidiMessage::controllerEvent (2, 74, 127), 0);
            f.process (memberCC);
            TestUtils::runMessageLoopFor (100);
            expectEquals ((float) sound.getProperty (SourceIDs::pan), SourceDefaults::pan);
            juce::MidiBuffer masterCC;
            masterCC.addEvent (juce::MidiMessage::controllerEvent (MPE_MASTER_CHANNEL, 74, 127), 0);
            f.process (masterCC);
            expect (TestUtils::runMessageLoopUntil ([&] { return (float) sound.getProperty (SourceIDs::pan) == 1.0f; }, 1000), "Master channel CC should apply CC mappings");
        }

        beginTest ("Voice info: one playhead per note, safe to poll while audio renders in another thread");
        {
            Fixture f;
            auto sound = f.addLocalSound ("a", 440.0);
            const auto uuid = sound.getProperty (SourceIDs::uuid).toString();
            f.send (ACTION_REAPPLY_LAYOUT, { juce::String (NOTE_MAPPING_TYPE_CONTIGUOUS) });  // Single sound -> all notes mapped

            // Audio thread: renders continuously (like a real audio callback)
            std::atomic<bool> stopAudio { false };
            std::thread audioThread ([&]
            {
                juce::AudioBuffer<float> buffer (2, blockSize);
                while (! stopAudio.load())
                {
                    juce::MidiBuffer midi;
                    f.sampler.processBlock (buffer, midi);
                    std::this_thread::sleep_for (std::chrono::milliseconds (2));
                }
            });

            // Two notes of the same sound at different pitches -> two playheads moving at different speeds
            f.send (ACTION_NOTE_ON, { "64", "127", "1" });
            f.send (ACTION_NOTE_ON, { "76", "127", "1" });
            int maxSimultaneous = 0;
            bool sawDifferentPositions = false;
            const auto end = juce::Time::getMillisecondCounter() + 600;
            while (juce::Time::getMillisecondCounter() < end)
            {
                std::vector<float> positions;
                for (const auto& voice : f.sampler.getVoicesInfo())
                {
                    if (voice.active && voice.soundUUID == uuid)
                    {
                        expect (voice.playPosition >= 0.0f && voice.playPosition <= 1.0f);
                        expectEquals (voice.firstSampleUUID, sound.getChildWithName (SourceIDs::SOUND_SAMPLE).getProperty (SourceIDs::uuid).toString());
                        positions.push_back (voice.playPosition);
                    }
                }
                maxSimultaneous = juce::jmax (maxSimultaneous, (int) positions.size());
                if (positions.size() == 2 && std::abs (positions[0] - positions[1]) > 0.01f)
                    sawDifferentPositions = true;
                f.sampler.collectVolatileStateInformationAsString();  // Used by remote UIs, same data
                TestUtils::runMessageLoopFor (5);
            }

            // Remove the sound while voices may still be playing and keep polling (must not crash or report stale sounds)
            f.send (ACTION_REMOVE_SOUND, { uuid });
            const auto removeEnd = juce::Time::getMillisecondCounter() + 500;
            while (juce::Time::getMillisecondCounter() < removeEnd)
            {
                for (const auto& voice : f.sampler.getVoicesInfo())
                    expect (! voice.active || voice.soundUUID == uuid);
                TestUtils::runMessageLoopFor (5);
            }
            stopAudio = true;
            audioThread.join();

            expectEquals (maxSimultaneous, 2, "Expected two simultaneous playheads for two notes");
            expect (sawDifferentPositions, "Playheads of notes at different pitches should move at different speeds");
            expectEquals (f.numSounds(), 0);
            for (const auto& voice : f.sampler.getVoicesInfo())
                expect (! voice.active);
        }

        beginTest ("Session autosave (iOS standalone) saves after changes and restores on the next launch");
        {
            TestUtils::TemporaryFolder tmp;
            auto sessionFile = tmp.folder.getChildFile ("last_session.xml");
            auto wav = TestUtils::writeSineWav (tmp.folder.getChildFile ("s.wav"), 440.0, 0.3, sampleRate);
            {
                SourceSampler sampler (tmp.folder);
                sampler.enableSessionAutosave (sessionFile);
                TestUtils::runMessageLoopFor (50);
                expect (! sessionFile.existsAsFile(), "Nothing to save yet");
                sampler.actionListenerCallback (SourceHelpers::serializeAction (ACTION_ADD_SOUND, { "-1", "autosaved", "", "", "", wav.getFullPathName(), "wav", juce::String (wav.getSize()), "", "", "60" }));
                sampler.actionListenerCallback (SourceHelpers::serializeAction (ACTION_SET_SOUND_PARAMETER_FLOAT, { "", "gain", "-5" }));
                expect (TestUtils::runMessageLoopUntil ([&] { return sessionFile.existsAsFile(); }, 5000), "Session was not autosaved");
            }
            SourceSampler sampler (tmp.folder);
            sampler.enableSessionAutosave (sessionFile);
            auto preset = [&] { return sampler.state.getChildWithName (SourceIDs::PRESET); };
            expect (TestUtils::runMessageLoopUntil ([&] { return preset().getNumChildren() == 1; }, 2000), "Session was not restored");
            auto sound = preset().getChild (0);
            expectEquals (sound.getChildWithName (SourceIDs::SOUND_SAMPLE).getProperty (SourceIDs::name).toString(), juce::String ("autosaved"));
            expectEquals ((float) sound.getProperty (SourceIDs::gain), -5.0f);
            expect (TestUtils::runMessageLoopUntil ([sound] { return (bool) sound.getProperty (SourceIDs::allSoundsLoaded); }), "Restored sound not loaded");
        }

        beginTest ("Preset import folder (iOS standalone) moves valid preset files to the presets folder");
        {
            Fixture f;
            f.addLocalSound ("a", 440.0);
            f.send (ACTION_SAVE_PRESET, { "Imported preset", "7" });
            auto savedPreset = f.sampler.presetFilesLocation.getChildFile ("7.xml");
            expect (savedPreset.existsAsFile());

            TestUtils::TemporaryFolder importTmp;
            auto importFolder = importTmp.folder.getChildFile ("presets");
            importFolder.createDirectory();
            expect (savedPreset.moveFileTo (importFolder.getChildFile ("12.xml")));
            importFolder.getChildFile ("notAPreset.xml").replaceWithText ("<PRESET/>");
            importFolder.getChildFile ("13.xml").replaceWithText ("<SomethingElse/>");
            importFolder.getChildFile ("14.xml").replaceWithText ("not xml");

            f.sampler.enablePresetImportFolder (importFolder);
            expect (importFolder.getChildFile ("README.txt").existsAsFile());
            expect (f.sampler.presetFilesLocation.getChildFile ("12.xml").existsAsFile(), "Valid preset was not imported");
            expect (! importFolder.getChildFile ("12.xml").exists(), "Imported preset is moved, not copied");
            for (auto name : { "notAPreset.xml", "13.xml", "14.xml" })
            {
                expect (importFolder.getChildFile (name).existsAsFile(), juce::String ("Invalid file should stay: ") + name);
                expect (! f.sampler.presetFilesLocation.getChildFile (name).exists(), juce::String ("Invalid file imported: ") + name);
            }

            // Files added later are picked up by the timer
            expect (f.sampler.presetFilesLocation.getChildFile ("12.xml").copyFileTo (importFolder.getChildFile ("15.xml")));
            expect (TestUtils::runMessageLoopUntil ([&] { return f.sampler.presetFilesLocation.getChildFile ("15.xml").existsAsFile(); }, 5000),
                    "Preset added later was not imported");

            f.send (ACTION_LOAD_PRESET, { "12" });
            expectEquals (f.preset().getProperty (SourceIDs::name).toString(), juce::String ("Imported preset"));

            // The presets folder itself is never used as import folder
            f.sampler.enablePresetImportFolder (f.sampler.presetFilesLocation);
            expect (f.sampler.getPresetImportFolder() == importFolder);
        }

        beginTest ("Failed downloads don't block loading and never block the message thread");
        {
            Fixture f;
            // A Freesound sound whose preview can't be downloaded (nothing listens on that port)
            const auto start = juce::Time::getMillisecondCounterHiRes();
            f.send (ACTION_ADD_SOUND, { "123456", "unreachable", "user", "license", "http://127.0.0.1:9/unreachable.ogg", "", "ogg", "1000", "", "", "60" });
            expect (juce::Time::getMillisecondCounterHiRes() - start < 500.0, "Adding a sound must not wait for the download");
            auto sound = f.preset().getChild (0);
            expect (TestUtils::runMessageLoopUntil ([sound] { return (bool) sound.getProperty (SourceIDs::allSoundsLoaded); }, 20000),
                    "Sound never finished loading after a failed download");
            auto sample = sound.getChildWithName (SourceIDs::SOUND_SAMPLE);
            expect ((bool) sample.getProperty (SourceIDs::downloadFailed));
            expect (! (bool) sample.getProperty (SourceIDs::downloadCompleted));
            expect (! f.sampler.sourceDataLocation.getChildFile (sample.getProperty (SourceIDs::filePath).toString()).existsAsFile());

            // Retrying downloads the file again (and fails again here)
            f.send (ACTION_RETRY_SOUND_DOWNLOADS, { sound.getProperty (SourceIDs::uuid).toString() });
            expect (! (bool) sample.getProperty (SourceIDs::downloadFailed), "Retrying clears the failed state");
            expect (! (bool) sound.getProperty (SourceIDs::allSoundsLoaded), "Retrying loads the sound again");
            expect (TestUtils::runMessageLoopUntil ([sample] { return (bool) sample.getProperty (SourceIDs::downloadFailed); }, 20000),
                    "Retried download did not run");
            expect (TestUtils::runMessageLoopUntil ([sound] { return (bool) sound.getProperty (SourceIDs::allSoundsLoaded); }, 20000));
        }

        beginTest ("Missing local files are marked when loading a preset");
        {
            Fixture f;
            auto sound = f.addLocalSound ("local", 440.0);
            auto sample = sound.getChildWithName (SourceIDs::SOUND_SAMPLE);
            expect (! (bool) sample.getProperty (SourceIDs::localFileMissing, false));
            auto file = f.sampler.sourceDataLocation.getChildFile (sample.getProperty (SourceIDs::filePath).toString());
            expect (file.existsAsFile());
            f.send (ACTION_SAVE_PRESET, { "With local file", "9" });
            expect (file.deleteFile());

            f.send (ACTION_LOAD_PRESET, { "9" });
            auto loadedSound = f.preset().getChild (0);
            expect (TestUtils::runMessageLoopUntil ([loadedSound] { return (bool) loadedSound.getProperty (SourceIDs::allSoundsLoaded); }, 5000));
            auto loadedSample = loadedSound.getChildWithName (SourceIDs::SOUND_SAMPLE);
            expect ((bool) loadedSample.getProperty (SourceIDs::localFileMissing));
            expect (! (bool) loadedSample.getProperty (SourceIDs::downloadCompleted), "Saved as completed, but the file is missing");
            expect (! (bool) loadedSample.getProperty (SourceIDs::downloadFailed, false), "Local files are not downloads");
            expectEquals (f.sampler.getSoundWithUUID (loadedSound.getProperty (SourceIDs::uuid).toString())->getLinkedSourceSamplerSounds().size(), (size_t) 0);
        }

        beginTest ("Replacing the audio of a sound keeps its settings and mappings");
        {
            Fixture f;
            auto sound = f.addLocalSound ("original", 440.0);
            const auto uuid = sound.getProperty (SourceIDs::uuid).toString();
            f.send (ACTION_SET_SOUND_PARAMETER_FLOAT, { uuid, "gain", "-7" });
            f.send (ACTION_SET_SOUND_PARAMETER_FLOAT, { uuid, "pitch", "3" });
            f.send (ACTION_ADD_OR_UPDATE_CC_MAPPING, { uuid, juce::Uuid().toString(), "20", "pan", "0.0", "1.0" });
            const auto notes = sound.getProperty (SourceIDs::midiNotes).toString();
            const int rootNote = sound.getChildWithName (SourceIDs::SOUND_SAMPLE).getProperty (SourceIDs::midiRootNote);

            auto newFile = TestUtils::writeSineWav (f.tmp.folder.getChildFile ("replacement.wav"), 220.0, 0.5, sampleRate);
            f.send (ACTION_REPLACE_SOUND_AUDIO, { uuid, "-1", "replacement", "", "", "", newFile.getFullPathName(), "wav", juce::String (newFile.getSize()), "" });
            expectEquals (f.numSounds(), 1);
            auto replaced = f.preset().getChild (0);
            expect (replaced.getProperty (SourceIDs::uuid).toString() != uuid, "The replaced sound is a new sound");
            expectEquals (replaced.getChildWithName (SourceIDs::SOUND_SAMPLE).getProperty (SourceIDs::name).toString(), juce::String ("replacement"));
            expectEquals ((float) replaced.getProperty (SourceIDs::gain), -7.0f);
            expectEquals ((float) replaced.getProperty (SourceIDs::pitch), 3.0f);
            expectEquals (replaced.getProperty (SourceIDs::midiNotes).toString(), notes);
            expectEquals ((int) replaced.getChildWithName (SourceIDs::SOUND_SAMPLE).getProperty (SourceIDs::midiRootNote), rootNote);
            expect (replaced.getChildWithName (SourceIDs::MIDI_CC_MAPPING).isValid());
            expect (TestUtils::runMessageLoopUntil ([replaced] { return (bool) replaced.getProperty (SourceIDs::allSoundsLoaded); }));

            f.send (ACTION_REPLACE_SOUND_AUDIO, { "not-a-sound", "-1", "x", "", "", "", newFile.getFullPathName(), "wav", "1", "" });
            expectEquals (f.numSounds(), 1);
        }

        beginTest ("Muting silences the output (with a short fade)");
        {
            Fixture f;
            auto sound = f.addLocalSound ("loud", 440.0);
            f.send (ACTION_PLAY_SOUND, { sound.getProperty (SourceIDs::uuid).toString() });
            juce::AudioBuffer<float> buffer (2, blockSize);
            juce::MidiBuffer midi;
            f.sampler.processBlock (buffer, midi);
            f.sampler.processBlock (buffer, midi);
            expectGreaterThan (buffer.getMagnitude (0, blockSize), 0.01f);
            f.sampler.setOutputMuted (true);
            for (int i = 0; i < 10; ++i)
                f.sampler.processBlock (buffer, midi);
            expectEquals (buffer.getMagnitude (0, blockSize), 0.0f);
            f.sampler.setOutputMuted (false);
            for (int i = 0; i < 10; ++i)
                f.sampler.processBlock (buffer, midi);
            expectGreaterThan (buffer.getMagnitude (0, blockSize), 0.01f);
        }

        beginTest ("Hover previews play polyphonically (up to 10)");
        {
            Fixture f;
            juce::AudioBuffer<float> buffer (2, blockSize);
            juce::MidiBuffer midi;
            for (int i = 0; i < 12; ++i)
            {
                auto file = TestUtils::writeSineWav (f.tmp.folder.getChildFile ("hover" + juce::String (i) + ".wav"), 220.0 + 20.0 * i, 1.0, sampleRate);
                f.send (ACTION_PLAY_HOVER_PREVIEW, { file.getFullPathName() });
                expectEquals (f.sampler.getNumHoverPreviewsPlaying(), juce::jmin (i + 1, 10));
            }
            f.sampler.processBlock (buffer, midi);
            expectGreaterThan (buffer.getMagnitude (0, blockSize), 0.1f);
            f.send (ACTION_PLAY_SOUND_FILE_FROM_PATH, { "" });  // Stopping previews stops these too
            expectEquals (f.sampler.getNumHoverPreviewsPlaying(), 0);
        }

        beginTest ("Presets in the old format keep their Freesound onsets (as manual slices when numSlices was 0)");
        {
            Fixture f;
            const juce::String oldPreset = R"(<?xml version="1.0" encoding="UTF-8"?>
<SourcePresetState presetName="old" noteLayoutType="0">
  <soundsInfo>
    <soundInfo soundId="1" soundName="a" soundUser="u" soundLicense="" soundOGGURL="">
      <fsAnalysis><onsets><onset time="0.001"/><onset time="0.5"/><onset time="1.25"/></onsets></fsAnalysis>
      <SamplerSound midiNotes="ff" soundIdx="0">
        <SamplerSoundParameter parameter_type="int" parameter_name="numSlices" parameter_value="0"/>
      </SamplerSound>
    </soundInfo>
    <soundInfo soundId="2" soundName="b" soundUser="u" soundLicense="" soundOGGURL="">
      <fsAnalysis><onsets/></fsAnalysis>
      <SamplerSound midiNotes="ff" soundIdx="0"/>
    </soundInfo>
  </soundsInfo>
</SourcePresetState>)";
            expect (f.sampler.presetFilesLocation.getChildFile ("40.xml").replaceWithText (oldPreset));
            f.send (ACTION_LOAD_PRESET, { "40" });
            auto first = f.preset().getChild (0), second = f.preset().getChild (1);
            expect (first.hasType (SourceIDs::SOUND) && second.hasType (SourceIDs::SOUND));
            expectEquals (first.getChildWithName (SourceIDs::SOUND_SAMPLE).getChildWithName (SourceIDs::ANALYSIS)
                               .getChildWithName (SourceIDs::onsets).getNumChildren(), 3, "Onsets kept");
            expectEquals (first.getProperty (SourceIDs::slices).toString(), juce::String ("0.5000,1.2500"), "Manual slices from the onsets");
            expectEquals (second.getProperty (SourceIDs::slices).toString(), juce::String(), "No onsets, no slices");
        }

        beginTest ("Program changes load presets");
        {
            Fixture f;
            f.addLocalSound ("a", 440.0);
            f.send (ACTION_SAVE_PRESET, { "PC preset", "2" });
            f.send (ACTION_LOAD_PRESET, { "9" });
            juce::MidiBuffer pc;
            pc.addEvent (juce::MidiMessage::programChange (1, 2), 0);
            f.process (pc);
            expect (TestUtils::runMessageLoopUntil ([&] { return (int) f.sampler.state.getProperty (SourceIDs::currentPresetIndex) == 2; }, 1000));
            expectEquals (f.preset().getProperty (SourceIDs::name).toString(), juce::String ("PC preset"));
        }
    }
};

static SourceSamplerTests sourceSamplerTests;
