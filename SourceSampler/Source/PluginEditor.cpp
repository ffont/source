/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryData.h"
#if INCLUDE_SEQUENCER
#include "defines_shepherd.h"
#endif


//==============================================================================
SourceSamplerAudioProcessorEditor::SourceSamplerAudioProcessorEditor (SourceSamplerAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    processor.source.addActionListener(this);
    
    // Copy bundled HTML plugin file to dcuments folder so we can load it
    #if ELK_BUILD
    juce::File baseLocation = juce::File(ELK_SOURCE_TMP_LOCATION);
    #else
    #if USE_APP_GROUP_ID
    juce::File baseLocation = juce::File::getContainerForSecurityApplicationGroupIdentifier(APP_GROUP_ID).getChildFile((juce::String)SOURCE_APP_DIRECTORY_NAME + "/tmp");
    #else
    juce::File baseLocation = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile((juce::String)SOURCE_APP_DIRECTORY_NAME + "/tmp");
    #endif
    
    #endif
    if (!baseLocation.exists()){
        baseLocation.createDirectory();
    }
    juce::File uiHtmlFile = baseLocation.getChildFile("ui_plugin_ws").withFileExtension("html");
    #if USE_LAZY_UI
    uiHtmlFile.replaceWithData(BinaryData::ui_plugin_ws_lazy_html , BinaryData::ui_plugin_ws_lazy_htmlSize);
    #else
    uiHtmlFile.replaceWithData(BinaryData::ui_plugin_ws_html , BinaryData::ui_plugin_ws_htmlSize);
    #endif
    uiHTMLFilePath = uiHtmlFile.getFullPathName();
    
    openInBrowser.addListener(this);
    openInBrowser.setButtonText("Open UI in browser");
    addAndMakeVisible (openInBrowser);
    
    #if INCLUDE_SEQUENCER
    openSequencerInBrowser.addListener(this);
    openSequencerInBrowser.setButtonText("Open sequencer UI in browser");
    addAndMakeVisible (openSequencerInBrowser);
    #endif
    
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
        browser.goToURL(makeUIURL().toString(true));
    #endif
    
    setSize(10, 10);  // This is reset later
    setResizable(false, false);
}

SourceSamplerAudioProcessorEditor::~SourceSamplerAudioProcessorEditor()
{
    processor.source.removeActionListener(this);
    
    #if !ELK_BUILD
    browser.editor.release();
    #endif
}

juce::URL SourceSamplerAudioProcessorEditor::makeUIURL(){
    #if USE_SSL_FOR_HTTP_AND_WS
    juce::String useWss = "1";
    #else
    juce::String useWss = "0";
    #endif
    #if INCLUDE_SEQUENCER
    juce::String withSequencer = "1";
    #else
    juce::String withSequencer = "0";
    #endif
    #if USING_DIRECT_COMMUNICATION_METHOD
    bool useDC = true;
    #else
    bool useDC = false;
    #endif
    juce::URL url = juce::URL("file://" + uiHTMLFilePath).withParameter("wsPort", (juce::String)processor.source.getServerInterfaceWSPort()).withParameter("useWss", useWss).withParameter("httpPort", (juce::String)processor.source.getServerInterfaceHttpPort()).withParameter("withSequencer", withSequencer).withParameter("useDC", useDC ? "1": "0");
    DBG("> Satic UI URL: " << url.toString(true));
    return url;
}

void SourceSamplerAudioProcessorEditor::buttonClicked (juce::Button* button){
    if (button == &openInBrowser){
        makeUIURL().launchInDefaultBrowser();
    #if INCLUDE_SEQUENCER
    } else if (button == &openSequencerInBrowser){
        juce::URL(DEV_UI_SIMULATOR_URL).launchInDefaultBrowser();
    #endif
    } else if (button == &reloadUI){
        #if !ELK_BUILD
            hadBrowserError = false; // Reset browser error property
            browser.goToURL(makeUIURL().toString(true));
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
        #if !JUCE_IOS
        float width = 980;
        float height = 800;
        #else
        float width = getWidth();
        float height = getHeight();
        #endif
        #if JUCE_DEBUG && !JUCE_IOS
        setSize(width, height + 20);
        openInBrowser.setBounds(5, 0, 150, 20);
        #if INCLUDE_SEQUENCER
        openSequencerInBrowser.setBounds(160, 0, 150, 20);
        #endif
        browser.setBounds(0, 20, width, height);
        #else
        setSize(width, height);
        browser.setBounds(0, 0, width, height);
        #endif
    } else {
        browser.setBounds(0, 0, 0, 0);
        explanation.setBounds(10, 10, 590, 150);
        openInBrowser.setBounds(10, 160, 150, 20);
        #if INCLUDE_SEQUENCER
        openSequencerInBrowser.setBounds(165, 160, 150, 20);
        #endif
        reloadUI.setBounds(320, 160, 90, 20);
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

void SourceSamplerAudioProcessorEditor::actionListenerCallback (const juce::String &message)
{
    #if !ELK_BUILD
    browser.sendMessage(message);
    #endif
}
