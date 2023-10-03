/*
  ==============================================================================

    SourceSamplerVoice.h
    Created: 3 Sep 2020 2:13:36pm
    Author:  Frederic Font Corbera

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "helpers_source.h"
#include "SourceSamplerVoice.h"
#include "SourceSamplerSound.h"



class SourceSamplerSynthesiser: public juce::MPESynthesiser,
                                public juce::ActionBroadcaster
{
public:
    SourceSamplerSynthesiser();
    
    static constexpr auto maxNumVoices = 32;
    void setSamplerVoices(int nVoices);
    void prepare (const juce::dsp::ProcessSpec& spec) noexcept;
    
    //==============================================================================
    void noteOn (const int midiChannel,
                 const int midiNoteNumber,
                 const float velocity);
    void handleMidiEvent2 (const juce::MidiMessage& m);
    
    //==============================================================================
    void setReverbParameters (juce::Reverb::Parameters params);
    
private:
    //==============================================================================
    void renderNextSubBlock (juce::AudioBuffer< float > &outputAudio, int startSample, int numSamples) override;

    enum
    {
        reverbIndex
    };
    int lastModWheelValue = 0;
    int currentNumChannels = 0;
    int currentBlockSize = 0;
    juce::dsp::ProcessorChain<juce::dsp::Reverb> fxChain;
    
    bool mpeEnabled = false;
};
