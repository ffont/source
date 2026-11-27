/*
  ==============================================================================

    SettingsPanel.h

    "Settings" tab of the standalone app on iPad: the audio and MIDI settings
    of the standalone app (the same as the "Audio/MIDI Settings" window of the
    desktop standalone app, which can't be opened on iPad), including the
    "Bluetooth MIDI" button to pair Bluetooth MIDI controllers.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "SourceLookAndFeel.h"


class SettingsPanel : public juce::Component,
                      private juce::ChangeListener
{
public:
    /** saveSettings is called when the settings change (so they are restored the next time the app is opened). */
    SettingsPanel (juce::AudioDeviceManager& manager, int maxNumOutputChannels, std::function<void()> saveSettings)
        : deviceManager (manager), onSettingsChanged (std::move (saveSettings)),
          selector (manager,
                    0, 0,                     // No audio inputs (SOURCE is an instrument)
                    1, juce::jmax (2, maxNumOutputChannels),
                    true,                     // MIDI inputs (with the "Bluetooth MIDI" button where available)
                    false,                    // No MIDI output selector
                    true,                     // Channels as stereo pairs
                    false)                    // Show the advanced options (sample rate, buffer size)
    {
        addAndMakeVisible (card);
        viewport.setViewedComponent (&selector, false);
        viewport.setScrollBarsShown (true, false);
        card.addAndMakeVisible (viewport);
        deviceManager.addChangeListener (this);
    }

    ~SettingsPanel() override
    {
        deviceManager.removeChangeListener (this);
    }

    void resized() override
    {
        // A centred column (the selector is laid out for widths around 500 px)
        card.setBounds (getLocalBounds().withSizeKeepingCentre (juce::jmin (getWidth(), 640), getHeight()));
        auto content = card.getContentBounds();
        viewport.setBounds (content);
        selector.setSize (content.getWidth() - viewport.getScrollBarThickness(), juce::jmax (content.getHeight(), 520));
    }

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override
    {
        if (onSettingsChanged != nullptr)
            onSettingsChanged();
    }

    juce::AudioDeviceManager& deviceManager;
    std::function<void()> onSettingsChanged;
    CardComponent card { "Audio & MIDI settings" };
    juce::Viewport viewport;
    juce::AudioDeviceSelectorComponent selector;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SettingsPanel)
};
