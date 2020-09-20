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
    int port = processor.getServerInterfaceHttpPort();
    //browser.goToURL("http://localhost:" + (String)port  + "/index");
    browser.goToURL("https://localhost:" + (String)port  + "/index");
    #endif
    
    #if JUCE_MAC
    openInBrowser.addListener(this);
    openInBrowser.setButtonText("Open UI in browser");
    addAndMakeVisible (openInBrowser);
    #endif
    
    float width = 1000;
    float height = 800;
    setSize(width, height);
    setResizable(false, false);
}

SourceSamplerAudioProcessorEditor::~SourceSamplerAudioProcessorEditor()
{
}

void SourceSamplerAudioProcessorEditor::buttonClicked (Button* button){
    if (button == &openInBrowser)
    {
        int port = processor.getServerInterfaceHttpPort();
        URL("http://localhost:" + (String)port + "/index").launchInDefaultBrowser();
    }
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
        openInBrowser.setBounds(10, 10, 120, 20);
        setSize(600, 100);
        #else
            #if !ELK_BUILD
            browser.setBounds(getLocalBounds());
            #endif
        #endif
    }
}
