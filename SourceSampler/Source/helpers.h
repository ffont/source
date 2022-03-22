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
        
        juce::ValueTree preset (IDs::PRESET);
        Helpers::createUuidProperty (preset);
        preset.setProperty (IDs::name, juce::Time::getCurrentTime().formatted("%Y%m%d") + " unnamed", nullptr);
        
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
