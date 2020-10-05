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
    browser.setEditorPointer(this);
    addAndMakeVisible(browser);
    int port = processor.getServerInterfaceHttpPort();
    #ifdef CPPHTTPLIB_OPENSSL_SUPPORT
    browser.goToURL("https://localhost:" + (String)port  + "/");
    #else
    browser.goToURL("http://localhost:" + (String)port  + "/");
    #endif
    #endif
    
    openInBrowser.addListener(this);
    openInBrowser.setButtonText("Open UI in browser");
    addAndMakeVisible (openInBrowser);
    
    reloadUI.addListener(this);
    reloadUI.setButtonText("Reload UI");
    addAndMakeVisible (reloadUI);
    
    explanation.setText("Looks like there were some problems loading the UI, most probably due to untrusted SSL certificates. "
                        "Please, click the \"Open UI in browser\" button below to open Source UI in your default web browser. "
                        "Once there, you'll be asked whether or not you want to trust the SSL self-sertificate for localhost. "
                        "If you trust the certificate and continue loading the page, you should see a fully usable Source UI in "
                        "your browser. Close your browser, come back to this window and click \"Reload UI\". You should now be "
                        "seeing the plugin UI normally. Once the certificate is trusted, you should not be seeing this problem again "
                        "when using Source in the same computer. If this does not fix the problem but you can see the UI in the browser, "
                        "you can use the UI from there (Source will work fine).", dontSendNotification);
    explanation.setJustificationType(Justification::topLeft);
    addAndMakeVisible (explanation);
    
    setSize(10, 10);  // This is reset later
    setResizable(false, false);
}

SourceSamplerAudioProcessorEditor::~SourceSamplerAudioProcessorEditor()
{
    #if !ELK_BUILD
    browser.editor.release();
    #endif
}

void SourceSamplerAudioProcessorEditor::buttonClicked (Button* button){
    if (button == &openInBrowser)
    {
        int port = processor.getServerInterfaceHttpPort();
        #ifdef CPPHTTPLIB_OPENSSL_SUPPORT
        URL("https://localhost:" + (String)port + "/").launchInDefaultBrowser();
        #else
        URL("http://localhost:" + (String)port + "/").launchInDefaultBrowser();
        #endif
    } else if (button == &reloadUI)
    {
        #if !ELK_BUILD
            hadBrowserError = false; // Reset browser error property
            int port = processor.getServerInterfaceHttpPort();
            #ifdef CPPHTTPLIB_OPENSSL_SUPPORT
            browser.goToURL("https://localhost:" + (String)port  + "/");
            #else
            browser.goToURL("http://localhost:" + (String)port  + "/");
            #endif
            resized();
        #endif
    }
}

//==============================================================================
void SourceSamplerAudioProcessorEditor::paint (Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));
}

void SourceSamplerAudioProcessorEditor::resized()
{
    #if !ELK_BUILD
    if (!hadBrowserError){
        float width = 1050;
        float height = 800;
        setSize(width, height);
        browser.setBounds(getLocalBounds());
    } else {
        browser.setBounds(0, 0, 0, 0);
        explanation.setBounds(10, 10, 590, 150);
        openInBrowser.setBounds(10, 160, 150, 20);
        reloadUI.setBounds(170, 160, 90, 20);
        setSize(600, 190);
    }
    #endif
    
    /*
    // NOTE: the code below was to make a "open UI in browser" button when running as a
    // plugin and in MACOS because of issues with the non-https server. Now I have implemented
    // an https server so this is not needed. Still, the self-signed certificate needs to be
    // approved using a real browser or the WebBrowserComponent won't load the UI.
     
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
    }*/
}
