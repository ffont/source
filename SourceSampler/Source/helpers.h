/*
  ==============================================================================

    helpers.h
    Created: 22 Mar 2022 10:12:20pm
    Author:  Frederic Font Corbera

  ==============================================================================
*/



#pragma once

#include <JuceHeader.h>
#include "defines.h"
#include "drow_ValueTreeObjectList.h"

namespace Helpers
{

    inline juce::ValueTree createUuidProperty (juce::ValueTree& v)
    {
        if (! v.hasProperty (IDs::uuid))
            v.setProperty (IDs::uuid, juce::Uuid().toString(), nullptr);
        return v;
    }

    inline juce::ValueTree createEmptyState()
    {
        juce::ValueTree state (IDs::SOURCE_STATE);
        Helpers::createUuidProperty (state);
        state.setProperty (IDs::currentPresetIndex, Defaults::currentPresetIndex, nullptr);
        return state;
    }

    inline juce::ValueTree createEmptyPresetState()
    {
        juce::ValueTree preset (IDs::PRESET);
        Helpers::createUuidProperty (preset);
        preset.setProperty (IDs::name, juce::Time::getCurrentTime().formatted("%Y%m%d") + " unnamed", nullptr);
        preset.setProperty (IDs::noteLayoutType, Defaults::noteLayoutType, nullptr);
        return preset;
    }

    inline juce::ValueTree createEmptySourceSoundState(const juce::String soundName)
    {
        juce::ValueTree sound (IDs::SOUND);
        Helpers::createUuidProperty (sound);
        sound.setProperty (IDs::name, soundName, nullptr);
        sound.setProperty (IDs::enabled, true, nullptr);
        return sound;
    }

    inline juce::ValueTree createSourceSampleSoundState(int soundId, const juce::String previewURL)
    {
        juce::ValueTree ss (IDs::SOUND_SAMPLE);
        Helpers::createUuidProperty (ss);
        ss.setProperty (IDs::soundId, soundId, nullptr);
        ss.setProperty (IDs::previewURL, previewURL, nullptr);
        ss.setProperty(IDs::midiRootNote, 64, nullptr);
        BigInteger midiNotes;
        midiNotes.setRange(0, 127, true);
        ss.setProperty(IDs::midiNotes, midiNotes.toString(16), nullptr);
        return ss;
    }

    inline juce::ValueTree createAnalysisFromFreesoundAnalysis(juce::var fsAnalysisValueTree)
    {
        juce::ValueTree soundAnalysis = juce::ValueTree(IDs::ANALYSIS);
        if (fsAnalysisValueTree.hasProperty("rhythm")){
            if (fsAnalysisValueTree["rhythm"].hasProperty("onset_times")){
                juce::ValueTree soundAnalysisOnsetTimes = juce::ValueTree(IDs::onsets);
                for (int j=0; j<fsAnalysisValueTree["rhythm"]["onset_times"].size(); j++){
                    ValueTree onset = ValueTree(IDs::onset);
                    onset.setProperty(IDs::onsetTime, fsAnalysisValueTree["rhythm"]["onset_times"][j], nullptr);
                    soundAnalysisOnsetTimes.appendChild(onset, nullptr);
                }
                soundAnalysis.appendChild(soundAnalysisOnsetTimes, nullptr);
            }
        }
        return soundAnalysis;
    }

    inline juce::ValueTree createDefaultState(int numSounds)
    {
        juce::ValueTree state = createEmptyState();
        juce::ValueTree preset = createEmptyPresetState();
    
        for (int sn = 0; sn < numSounds; ++sn)
        {
            juce::ValueTree s = createEmptySourceSoundState("Sound " + juce::String (sn + 1));
            juce::ValueTree ss = createSourceSampleSoundState(433328, "https://freesound.org/data/previews/433/433328_735175-hq.ogg");
            s.addChild(ss, -1, nullptr);
            preset.addChild (s, -1, nullptr);
        }
        state.addChild (preset, -1, nullptr);

        return state;
    }
}


class MidiCCMapping
{
public:
    
    MidiCCMapping(int _ccNumber, String _parameterName, float _minRange, float _maxRange){
        randomID = juce::Random::getSystemRandom().nextInt (99999);
        ccNumber = _ccNumber;
        parameterName = _parameterName;
        minRange = _minRange;
        maxRange = _maxRange;
    }
    
    bool operator< (const MidiCCMapping &other) const {
        if (ccNumber != other.ccNumber){
            return ccNumber < other.ccNumber;
        } else {
            return parameterName < other.parameterName;
        }
    }
    
    ValueTree getState(){
        ValueTree state = ValueTree(STATE_SAMPLER_SOUND_MIDI_CC_MAPPING);
        state.setProperty(STATE_SAMPLER_SOUND_MIDI_CC_MAPPING_RANDOM_ID, randomID, nullptr);  // This number is serialized so the UI can use it, but when creating a new object a new random ID is generated
        state.setProperty(STATE_SAMPLER_SOUND_MIDI_CC_MAPPING_NUMBER, ccNumber, nullptr);
        state.setProperty(STATE_SAMPLER_SOUND_MIDI_CC_MAPPING_NAME, parameterName, nullptr);
        state.setProperty(STATE_SAMPLER_SOUND_MIDI_CC_MAPPING_MIN, minRange, nullptr);
        state.setProperty(STATE_SAMPLER_SOUND_MIDI_CC_MAPPING_MAX, maxRange, nullptr);
        return state;
    }
    
    int randomID;
    int ccNumber;
    String parameterName;
    float minRange;
    float maxRange;
};


struct GlobalContextStruct {
    double sampleRate = 0.0;
    int samplesPerBlock = 0;
    Synthesiser* sampler = nullptr;
    File sourceDataLocation;
    File soundsDownloadLocation;
    File presetFilesLocation;
    File tmpFilesLocation;
};
