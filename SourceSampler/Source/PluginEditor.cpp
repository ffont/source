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
    #if USE_WEBSOCKETS
    // Copy bundled HTML plugin file to dcuments folder so we can load it
    juce::File sourceDataLocation = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("SourceSampler/");
    juce::File uiHtmlFile = sourceDataLocation.getChildFile("ui_plugin_ws").withFileExtension("html");
    uiHtmlFile.replaceWithData(BinaryData::ui_plugin_ws_html, BinaryData::ui_plugin_ws_htmlSize);
    uiHTMLFilePath = uiHtmlFile.getFullPathName();
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
                        "you can use the UI from there (Source will work fine).", juce::dontSendNotification);
    explanation.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible (explanation);
    
    #if !ELK_BUILD
        browser.setEditorPointer(this);
        addAndMakeVisible(browser);
        #if USE_WEBSOCKETS
            browser.goToURL("file://" + uiHTMLFilePath + "?wsPort=" + (juce::String)processor.getServerInterfaceWSPort() + "&useWss=" + (juce::String)USE_WEBSOCKETS);
        #else
            int port = processor.getServerInterfaceHttpPort();
            #ifdef CPPHTTPLIB_OPENSSL_SUPPORT
            browser.goToURL("https://localhost:" + (juce::String)port  + "/");
            #else
            browser.goToURL("http://localhost:" + (juce::String)port  + "/");
            #endif
        #endif
    #endif
    
    setSize(10, 10);  // This is reset later
    setResizable(false, false);
}

SourceSamplerAudioProcessorEditor::~SourceSamplerAudioProcessorEditor()
{
    #if !ELK_BUILD
    browser.editor.release();
    #endif
}

void SourceSamplerAudioProcessorEditor::buttonClicked (juce::Button* button){
    if (button == &openInBrowser)
    {
        #if USE_WEBSOCKETS
            juce::URL("file://" + uiHTMLFilePath).withParameter("wsPort", (juce::String)processor.getServerInterfaceWSPort()).withParameter("useWss", (juce::String)USE_WEBSOCKETS).launchInDefaultBrowser();
        #else
            int port = processor.getServerInterfaceHttpPort();
            #ifdef CPPHTTPLIB_OPENSSL_SUPPORT
            juce::URL("https://localhost:" + (juce::String)port + "/").launchInDefaultBrowser();
            #else
            juce::URL("http://localhost:" + (juce::String)port + "/").launchInDefaultBrowser();
            #endif
        #endif
    } else if (button == &reloadUI)
    {
        #if !ELK_BUILD
            hadBrowserError = false; // Reset browser error property
            #if USE_WEBSOCKETS
                browser.goToURL("file://" + uiHTMLFilePath + "?wsPort=" + (juce::String)processor.getServerInterfaceWSPort() + "&useWss=" + (juce::String)USE_WEBSOCKETS);
            #else
                int port = processor.getServerInterfaceHttpPort();
                #ifdef CPPHTTPLIB_OPENSSL_SUPPORT
                browser.goToURL("https://localhost:" + (juce::String)port  + "/");
                #else
                browser.goToURL("http://localhost:" + (juce::String)port  + "/");
                #endif
            #endif
            resized();
        #endif
    }
}

//==============================================================================
void SourceSamplerAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void SourceSamplerAudioProcessorEditor::resized()
{
    #if !ELK_BUILD
    if (!hadBrowserError){
        float width = 933;
        float height = 800;
        #if JUCE_DEBUG
        setSize(width, height + 20);
        openInBrowser.setBounds(0, 0, 150, 20);
        browser.setBounds(0, 20, width, height);
        #else
        setSize(width, height);
        browser.setBounds(0, 0, width, height);
        #endif
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
