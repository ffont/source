/*
  ==============================================================================

    SourceSamplerVoice.h
    Created: 3 Sep 2020 2:13:36pm
    Author:  Frederic Font Corbera

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "defines.h"
#include "helpers.h"
#include "SourceSamplerVoice.h"
#include "SourceSamplerSound.h"



class SourceSamplerSynthesiser: public Synthesiser,
                                public ActionBroadcaster
{
public:
    SourceSamplerSynthesiser();
    
    static constexpr auto maxNumVoices = 32;
    void setSamplerVoices(int nVoices);
    void prepare (const juce::dsp::ProcessSpec& spec) noexcept;
    
    //==============================================================================
    void noteOn (const int midiChannel,
                 const int midiNoteNumber,
                 const float velocity) override;
    void handleMidiEvent (const MidiMessage& m) override;
    
    //==============================================================================
    int getMidiInChannel();
    void setMidiInChannel(int newMidiInChannel);
    void setReverbParameters (Reverb::Parameters params);
    
private:
    //==============================================================================
    void renderVoices (AudioBuffer< float > &outputAudio, int startSample, int numSamples) override;
    enum
    {
        reverbIndex
    };
    int midiInChannel = 0; // 0 all channels, 1=channel 1, 2=channel 2 etc
    int lastModWheelValue = 0;
    int currentNumChannels = 0;
    int currentBlockSize = 0;
    juce::dsp::ProcessorChain<juce::dsp::Reverb> fxChain;
};
