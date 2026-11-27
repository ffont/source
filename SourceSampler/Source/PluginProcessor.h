/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "SourceSampler.h"
#if SOURCE_LINK
#include "SourceLink.h"
#endif
#if INCLUDE_SEQUENCER
#include "Sequencer.h"
#endif


//==============================================================================
/**
*/
class SourceSamplerAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    SourceSamplerAudioProcessor();
    ~SourceSamplerAudioProcessor() override;
    
    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;
    
    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    
    SourceSampler source;
    
    /** On-screen keyboard state (notes played from the editor's keyboard are merged into the MIDI input). */
    juce::MidiKeyboardState keyboardState;
    
    #if SOURCE_LINK
    /** Ableton Link and Link Audio (only in the iPad standalone app, nullptr otherwise). */
    std::unique_ptr<SourceLink> link;
    #endif
    
    #if INCLUDE_SEQUENCER
    Sequencer sequencer;
    juce::MidiBuffer sequencerCombinedBuffer;
    #endif

private:

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SourceSamplerAudioProcessor)
};
