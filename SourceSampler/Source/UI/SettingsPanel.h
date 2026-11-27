/*
  ==============================================================================

    SettingsPanel.h

    "Settings" tab of the standalone app on iPad: the audio and MIDI settings
    of the standalone app (the same as the "Audio/MIDI Settings" window of the
    desktop standalone app, which can't be opened on iPad), including the
    "Bluetooth MIDI" button to pair Bluetooth MIDI controllers, and the Ableton
    Link / Link Audio settings (when SOURCE is built with Link, see SOURCE_LINK).

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "SourceLookAndFeel.h"
#if SOURCE_LINK
 #include "SourceLink.h"
#endif


#if SOURCE_LINK
/** Ableton Link settings: Link on/off, Link Audio on/off, the name of this device in the Link session and the
    number of connected peers. */
class LinkSettingsCard : public CardComponent,
                         private juce::Timer
{
public:
    explicit LinkSettingsCard (SourceLink& l) : CardComponent ("Ableton Link"), link (l)
    {
        linkToggle.setButtonText ("Link");
        linkToggle.setTooltip ("Join the Link session on the local network");
        linkToggle.onClick = [this] { link.setEnabled (linkToggle.getToggleState()); update(); };
        addAndMakeVisible (linkToggle);

        audioToggle.setButtonText ("Link Audio: send SOURCE's audio to other devices");
        audioToggle.setTooltip ("Other Link Audio peers (like Ableton Live) can receive SOURCE's output through the network");
        audioToggle.onClick = [this] { link.setAudioEnabled (audioToggle.getToggleState()); update(); };
        addAndMakeVisible (audioToggle);

        nameLabel.setText ("Name", juce::dontSendNotification);
        nameLabel.setColour (juce::Label::textColourId, SourceColours::dimText);
        addAndMakeVisible (nameLabel);
        nameEditor.setTooltip ("Name of this device in the Link session (as shown in Live)");
        nameEditor.setText (link.getPeerName(), false);
        nameEditor.onReturnKey = [this] { applyName(); nameEditor.unfocusAllComponents(); };
        nameEditor.onFocusLost = [this] { applyName(); };
        addAndMakeVisible (nameEditor);

        statusLabel.setColour (juce::Label::textColourId, SourceColours::dimText);
        addAndMakeVisible (statusLabel);
        noteLabel.setText ("Other devices are found through the local network: allow SOURCE to use it when asked. Finding them "
                           "needs the multicast entitlement (see DEVELOPERS.md).", juce::dontSendNotification);
        noteLabel.setColour (juce::Label::textColourId, SourceColours::dimText.withAlpha (0.8f));
        noteLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
        noteLabel.setJustificationType (juce::Justification::topLeft);
        addAndMakeVisible (noteLabel);

        update();
        startTimerHz (2);
    }

    static constexpr int preferredHeight = 26 + 16 + 4 * 30 + 34;

    void resized() override
    {
        auto c = getContentBounds();
        linkToggle.setBounds (c.removeFromTop (26).removeFromLeft (200));
        c.removeFromTop (4);
        audioToggle.setBounds (c.removeFromTop (26));
        c.removeFromTop (4);
        auto row = c.removeFromTop (26);
        nameLabel.setBounds (row.removeFromLeft (60));
        nameEditor.setBounds (row.removeFromLeft (juce::jmin (320, row.getWidth())));
        c.removeFromTop (4);
        statusLabel.setBounds (c.removeFromTop (26));
        c.removeFromTop (2);
        noteLabel.setBounds (c);
    }

private:
    void applyName()
    {
        link.setPeerName (nameEditor.getText());
        nameEditor.setText (link.getPeerName(), false);
    }

    void update()
    {
        linkToggle.setToggleState (link.isEnabled(), juce::dontSendNotification);
        audioToggle.setToggleState (link.isAudioEnabled(), juce::dontSendNotification);
        audioToggle.setEnabled (link.isEnabled());
        juce::String status;
        if (! link.isEnabled())
            status = "Link is off";
        else if (link.getNumPeers() == 0)
            status = "Looking for other Link devices...";
        else
            status = juce::String (link.getNumPeers()) + (link.getNumPeers() == 1 ? " device" : " devices") + " connected, "
                     + juce::String (link.getTempo(), 1) + " BPM";
        statusLabel.setText (status, juce::dontSendNotification);
    }

    void timerCallback() override { update(); }

    SourceLink& link;
    juce::ToggleButton linkToggle, audioToggle;
    juce::Label nameLabel, statusLabel, noteLabel;
    juce::TextEditor nameEditor;
};
#endif


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

   #if SOURCE_LINK
    /** Shows the Ableton Link settings below the audio and MIDI settings. */
    void setLink (SourceLink* newLink)
    {
        linkCard.reset();
        if (newLink != nullptr)
        {
            linkCard = std::make_unique<LinkSettingsCard> (*newLink);
            addAndMakeVisible (*linkCard);
        }
        resized();
    }
   #endif

    void resized() override
    {
        // A centred column (the selector is laid out for widths around 500 px)
        auto column = getLocalBounds().withSizeKeepingCentre (juce::jmin (getWidth(), 640), getHeight());
       #if SOURCE_LINK
        if (linkCard != nullptr)
        {
            linkCard->setBounds (column.removeFromBottom (juce::jmin (LinkSettingsCard::preferredHeight, column.getHeight() / 2)));
            column.removeFromBottom (8);
        }
       #endif
        card.setBounds (column);
        auto content = card.getContentBounds();
        viewport.setBounds (content);
        selector.setSize (content.getWidth() - viewport.getScrollBarThickness(), juce::jmax (content.getHeight(), 380));
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
   #if SOURCE_LINK
    std::unique_ptr<LinkSettingsCard> linkCard;
   #endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SettingsPanel)
};
