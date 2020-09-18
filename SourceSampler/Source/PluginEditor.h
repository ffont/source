/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/**
*/

class SourceSamplerAudioProcessorEditor  : public AudioProcessorEditor
{
public:
    SourceSamplerAudioProcessorEditor (SourceSamplerAudioProcessor&);
    ~SourceSamplerAudioProcessorEditor();

    //==============================================================================
    void paint (Graphics&) override;
    void resized() override;
    
    

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    SourceSamplerAudioProcessor& processor;
    #if !ELK_BUILD
    WebBrowserComponent browser;
    #endif
    Label uiNote; 

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SourceSamplerAudioProcessorEditor)
};
