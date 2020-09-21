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


class CustomWebBrowserComponent: public WebBrowserComponent
{
public:
    ~CustomWebBrowserComponent(){}
    
    void setEditorPointer(SourceSamplerAudioProcessorEditor* _editor){
         editor.reset(_editor);
    }
    
    inline bool pageLoadHadNetworkError (const String& errorInfo) override;
    
    std::unique_ptr<SourceSamplerAudioProcessorEditor> editor;
};


class SourceSamplerAudioProcessorEditor  : public AudioProcessorEditor,
                                           public Button::Listener
{
public:
    SourceSamplerAudioProcessorEditor (SourceSamplerAudioProcessor&);
    ~SourceSamplerAudioProcessorEditor();
    
    void buttonClicked (Button* button) override;

    //==============================================================================
    void paint (Graphics&) override;
    void resized() override;
    
    bool hadBrowserError = false;
    
    

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    SourceSamplerAudioProcessor& processor;
    #if !ELK_BUILD
    CustomWebBrowserComponent browser;  // ELK compialtion fails if using WebBrowserComponent
    #endif
    TextButton openInBrowser;
    TextButton reloadUI;
    Label explanation;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SourceSamplerAudioProcessorEditor)
};


bool CustomWebBrowserComponent::pageLoadHadNetworkError (const String& errorInfo)
{
    if (editor != nullptr){
        editor->hadBrowserError = true;
        editor->resized();
    }
    return true;
}
