/*
  ==============================================================================

    StandaloneApp.cpp

    Desktop standalone application (macOS, Linux, Windows). The same as JUCE's
    default standalone app (and the same settings file, so the saved audio/MIDI
    device setup is kept), but the window uses the native title bar of the OS
    and has no "Options" button: the audio and MIDI device settings are in
    SOURCE's "Settings" tab. Only compiled into the desktop Standalone target
    (with JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1, see CMakeLists.txt); the iPad
    app uses JUCE's default one, which has no title bar.

  ==============================================================================
*/

#include <JuceHeader.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

namespace
{

class SourceStandaloneWindow : public juce::StandaloneFilterWindow
{
public:
    SourceStandaloneWindow (const juce::String& title, std::unique_ptr<juce::StandalonePluginHolder> holder)
        : juce::StandaloneFilterWindow (title, juce::Colours::black, std::move (holder))
    {
        hideOptionsButton();
        setTitleBarButtonsRequired (juce::DocumentWindow::allButtons, false);
        setUsingNativeTitleBar (true);
    }

    void resized() override
    {
        juce::StandaloneFilterWindow::resized();
        hideOptionsButton();
    }

private:
    void hideOptionsButton()
    {
        // The window's own "Options" button (audio settings, save/load state) is not used
        for (auto* child : getChildren())
            if (auto* button = dynamic_cast<juce::TextButton*> (child))
                button->setVisible (false);
    }
};


class SourceStandaloneApp : public juce::JUCEApplication
{
public:
    SourceStandaloneApp()
    {
        juce::PropertiesFile::Options options;
        options.applicationName = juce::CharPointer_UTF8 (JucePlugin_Name);
        options.filenameSuffix = ".settings";
        options.osxLibrarySubFolder = "Application Support";
       #if JUCE_LINUX || JUCE_BSD
        options.folderName = "~/.config";
       #else
        options.folderName = "";
       #endif
        appProperties.setStorageParameters (options);
    }

    const juce::String getApplicationName() override           { return juce::CharPointer_UTF8 (JucePlugin_Name); }
    const juce::String getApplicationVersion() override        { return JucePlugin_VersionString; }
    bool moreThanOneInstanceAllowed() override                 { return true; }
    void anotherInstanceStarted (const juce::String&) override {}

    void initialise (const juce::String&) override
    {
        if (juce::Desktop::getInstance().getDisplays().displays.isEmpty())
        {
            pluginHolder = createPluginHolder();  // No displays: run without a window
            return;
        }
        mainWindow = std::make_unique<SourceStandaloneWindow> (getApplicationName(), createPluginHolder());
        mainWindow->setVisible (true);
    }

    void shutdown() override
    {
        pluginHolder = nullptr;
        mainWindow = nullptr;
        appProperties.saveIfNeeded();
    }

    void systemRequestedQuit() override
    {
        if (pluginHolder != nullptr)
            pluginHolder->savePluginState();
        if (mainWindow != nullptr)
            mainWindow->pluginHolder->savePluginState();

        if (juce::ModalComponentManager::getInstance()->cancelAllModalComponents())
        {
            juce::Timer::callAfterDelay (100, []
            {
                if (auto* app = juce::JUCEApplicationBase::getInstance())
                    app->systemRequestedQuit();
            });
        }
        else
        {
            quit();
        }
    }

private:
    std::unique_ptr<juce::StandalonePluginHolder> createPluginHolder()
    {
       #ifdef JucePlugin_PreferredChannelConfigurations
        constexpr juce::StandalonePluginHolder::PluginInOuts channels[] { JucePlugin_PreferredChannelConfigurations };
        const juce::Array<juce::StandalonePluginHolder::PluginInOuts> channelConfig (channels, juce::numElementsInArray (channels));
       #else
        const juce::Array<juce::StandalonePluginHolder::PluginInOuts> channelConfig;
       #endif
        return std::make_unique<juce::StandalonePluginHolder> (appProperties.getUserSettings(), false, juce::String {}, nullptr,
                                                               channelConfig, false);
    }

    juce::ApplicationProperties appProperties;
    std::unique_ptr<SourceStandaloneWindow> mainWindow;
    std::unique_ptr<juce::StandalonePluginHolder> pluginHolder;
};

} // namespace

juce::JUCEApplicationBase* juce_CreateApplication();
juce::JUCEApplicationBase* juce_CreateApplication() { return new SourceStandaloneApp(); }
