/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"


//==============================================================================
SourceSamplerAudioProcessorEditor::SourceSamplerAudioProcessorEditor (SourceSamplerAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    #if !ELK_BUILD
    addAndMakeVisible(browser);
    int port = processor.getServerInterfaceHttpPort() - 1;
    browser.goToURL("http://localhost:" + (String)port  + "/index");
    #endif
    
    #if JUCE_MAC
    uiNote.setJustificationType (Justification::left);
    uiNote.setText("Please point your browser at 'http://localhost:" + (String)port + "/index' to see the UI of the plugin", dontSendNotification);
    addAndMakeVisible (uiNote);
    #endif
    
    float width = 1000;
    float height = 800;
    setSize(width, height);
    setResizable(false, false);
}

SourceSamplerAudioProcessorEditor::~SourceSamplerAudioProcessorEditor()
{
}

//==============================================================================
void SourceSamplerAudioProcessorEditor::paint (Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));
}

void SourceSamplerAudioProcessorEditor::resized()
{
    
    if (juce::JUCEApplicationBase::isStandaloneApp()){
        #if !ELK_BUILD
        browser.setBounds(getLocalBounds());
        #endif
        
    } else {
        #if JUCE_MAC
        uiNote.setBounds(getLocalBounds());
        setSize(600, 100);
        #else
            #if !ELK_BUILD
            browser.setBounds(getLocalBounds());
            #endif
        #endif
    }
    
    
}
