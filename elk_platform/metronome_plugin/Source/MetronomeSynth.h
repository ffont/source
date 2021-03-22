/*
  ==============================================================================

    MetronomeSynth.h
    Created: 22 Mar 2021 3:27:51pm
    Author:  Frederic Font Corbera

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>

using namespace juce;

class MetronomeSynth : public Synthesiser {
public:
    void setup();
private:
    // manager object that finds an appropriate way to decode various audio files.  Used with SampleSound objects.
    AudioFormatManager audioFormatManager;
    
};
