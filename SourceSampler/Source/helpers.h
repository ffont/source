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

    inline juce::ValueTree createDefaultPreset(int numSounds)
    {
        juce::ValueTree state (IDs::SOURCE_STATE);
        Helpers::createUuidProperty (state);
        state.setProperty (IDs::currentPresetIndex, Defaults::currentPresetIndex, nullptr);
        
        juce::ValueTree preset (IDs::PRESET);
        Helpers::createUuidProperty (preset);
        preset.setProperty (IDs::name, juce::Time::getCurrentTime().formatted("%Y%m%d") + " unnamed", nullptr);
        preset.setProperty (IDs::noteLayoutType, Defaults::noteLayoutType, nullptr);
        
        for (int sn = 0; sn < numSounds; ++sn)
        {
            juce::ValueTree s (IDs::SOUND);
            const juce::String trackName ("Sound " + juce::String (sn + 1));
            Helpers::createUuidProperty (s);
            s.setProperty (IDs::name, trackName, nullptr);
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

