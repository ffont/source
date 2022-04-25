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

class SourceSamplerAudioProcessorEditor;

#if !ELK_BUILD
class CustomWebBrowserComponent: public juce::WebBrowserComponent
{
public:
    ~CustomWebBrowserComponent(){}
    
    void setEditorPointer(SourceSamplerAudioProcessorEditor* _editor){
         editor.reset(_editor);
    }
    
    inline bool pageLoadHadNetworkError (const juce::String& errorInfo) override;
    
    std::unique_ptr<SourceSamplerAudioProcessorEditor> editor;
};
#endif


class SourceSamplerAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                           public juce::Button::Listener
{
public:
    SourceSamplerAudioProcessorEditor (SourceSamplerAudioProcessor&);
    ~SourceSamplerAudioProcessorEditor();
    
    void buttonClicked (juce::Button* button) override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    
    bool hadBrowserError = false;
    
    

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    SourceSamplerAudioProcessor& processor;
    #if !ELK_BUILD
    CustomWebBrowserComponent browser;  // ELK compialtion fails if using WebBrowserComponent
    #endif
    juce::TextButton openInBrowser;
    juce::TextButton reloadUI;
    juce::Label explanation;
    juce::String uiHTMLFilePath;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SourceSamplerAudioProcessorEditor)
};


#if !ELK_BUILD
bool CustomWebBrowserComponent::pageLoadHadNetworkError (const juce::String& errorInfo)
{
    if (editor != nullptr){
        editor->hadBrowserError = true;
        editor->resized();
    }
    return true;
}
#endif
