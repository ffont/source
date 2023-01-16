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
    
    void setEditorPointer(SourceSamplerAudioProcessorEditor* _editor)
    {
         editor.reset(_editor);
    }
    
    void sendMessage (const juce::String message)
    {
        // Send message from backend to frontend
        juce::String url = "javascript:messageFromBackend('" + message + "')";
        goToURL (url);
    }
    
    inline bool pageAboutToLoad (const juce::String& newURL) override;
    inline bool pageLoadHadNetworkError (const juce::String& errorInfo) override;
    
    std::unique_ptr<SourceSamplerAudioProcessorEditor> editor;
    const juce::String urlSchema = "juce://";
};
#endif


class SourceSamplerAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                           public juce::Button::Listener,
                                           public juce::ActionListener
{
public:
    SourceSamplerAudioProcessorEditor (SourceSamplerAudioProcessor&);
    ~SourceSamplerAudioProcessorEditor();
    
    juce::URL makeUIURL();
    
    void buttonClicked (juce::Button* button) override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    
    bool hadBrowserError = false;
    
    void actionListenerCallback (const juce::String &message) override;
    
    void triggerActionInProcessor (const juce::String actionMessage) {
        processor.source.actionListenerCallback(actionMessage);
    }
    
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
    #if INCLUDE_SEQUENCER
    juce::TextButton openSequencerInBrowser;
    #endif

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

bool CustomWebBrowserComponent::pageAboutToLoad (const juce::String& newURL)
{
    if (newURL.startsWith (urlSchema)) {
        // Receive mesage from frontend to backend
        auto messageString = juce::URL::removeEscapeChars (newURL.substring (urlSchema.length()));
        #if USING_DIRECT_COMMUNICATION_METHOD
        if (editor != nullptr){
            editor->triggerActionInProcessor(messageString);
        }
        #endif
        return false;
    } else {
        return true;
    }
}
#endif
