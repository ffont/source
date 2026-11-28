/*
  ==============================================================================

    SettingsPanel.h

    "Settings" tab: MIDI settings (global MIDI input channel, MPE input),
    the audio and MIDI device settings of the standalone app (the same as the
    "Audio/MIDI Settings" window of the desktop standalone app, which can't be
    opened on iPad; including the "Bluetooth MIDI" button) and the Ableton Link /
    Link Audio settings (when SOURCE is built with Link, see SOURCE_LINK).

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "SourceLookAndFeel.h"
#if SOURCE_LINK
 #include "SourceLink.h"
#endif


/** Global MIDI settings: MIDI input channel and MPE input (with its per-note pitch bend range). */
class MidiSettingsCard : public CardComponent
{
public:
    explicit MidiSettingsCard (SourceActionSender actionSender) : CardComponent ("MIDI"), actions (std::move (actionSender))
    {
        auto setUpLabel = [this] (juce::Label& label, const juce::String& text)
        {
            label.setText (text, juce::dontSendNotification);
            label.setColour (juce::Label::textColourId, SourceColours::dimText);
            addAndMakeVisible (label);
        };
        setUpLabel (channelLabel, "MIDI in channel");
        channelCombo.addItem ("All", 1);
        for (int i = 1; i <= 16; ++i)
            channelCombo.addItem (juce::String (i), i + 1);
        channelCombo.setTooltip ("Channel of the MIDI input (sounds set to the global channel use it)");
        channelCombo.onChange = [this] { actions.send (ACTION_SET_GLOBAL_MIDI_IN_CHANNEL, { juce::String (channelCombo.getSelectedId() - 1) }); };
        addAndMakeVisible (channelCombo);

        mpeToggle.setTooltip ("Enable MPE input (lower zone: master channel 1, notes on channels 2-16). When enabled, all sounds respond "
                              "to notes on any channel, and pitch bend, pressure and slide (CC74) are applied per note");
        mpeToggle.onClick = [this] { sendMpeSettings(); };
        addAndMakeVisible (mpeToggle);
        setUpLabel (bendLabel, "Bend range");
        bendSlider.setSliderStyle (juce::Slider::IncDecButtons);
        bendSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 44, 20);
        bendSlider.setRange (1, 96, 1);
        bendSlider.setValue (SourceDefaults::mpePitchBendRange, juce::dontSendNotification);
        bendSlider.setIncDecButtonsMode (juce::Slider::incDecButtonsDraggable_Vertical);
        bendSlider.setTooltip ("MPE per-note pitch bend range (semitones)");
        bendSlider.onValueChange = [this] { sendMpeSettings(); };
        addAndMakeVisible (bendSlider);
    }

    static constexpr int preferredHeight = 26 + 16 + 2 * 30;

    void updateFromState (const juce::ValueTree& state)
    {
        channelCombo.setSelectedId ((int) state.getProperty (SourceIDs::globalMidiInChannel, 0) + 1, juce::dontSendNotification);
        const bool mpeEnabled = state.getProperty (SourceIDs::mpeEnabled, false);
        mpeToggle.setToggleState (mpeEnabled, juce::dontSendNotification);
        if (! bendSlider.isMouseButtonDown())
            bendSlider.setValue ((int) state.getProperty (SourceIDs::mpePitchBendRange, SourceDefaults::mpePitchBendRange), juce::dontSendNotification);
        bendSlider.setEnabled (mpeEnabled);
        bendLabel.setEnabled (mpeEnabled);
    }

    void resized() override
    {
        auto c = getContentBounds();
        auto row = c.removeFromTop (26);
        channelLabel.setBounds (row.removeFromLeft (120));
        channelCombo.setBounds (row.removeFromLeft (90).withSizeKeepingCentre (90, 22));
        c.removeFromTop (4);
        row = c.removeFromTop (26);
        mpeToggle.setBounds (row.removeFromLeft (120));
        bendLabel.setBounds (row.removeFromLeft (80));
        bendSlider.setBounds (row.removeFromLeft (110).withSizeKeepingCentre (110, 22));
    }

private:
    void sendMpeSettings()
    {
        actions.send (ACTION_SET_MPE_SETTINGS, { mpeToggle.getToggleState() ? "1" : "0", juce::String ((int) bendSlider.getValue()) });
    }

    SourceActionSender actions;
    juce::Label channelLabel, bendLabel;
    juce::ComboBox channelCombo;
    juce::ToggleButton mpeToggle { "MPE input" };
    juce::Slider bendSlider;
};


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
    /** deviceManager is the standalone app's device manager (nullptr in plugins: there are no device settings then).
        saveSettings is called when the device settings change (so they are restored the next time the app is opened). */
    SettingsPanel (SourceActionSender actionSender, juce::AudioDeviceManager* manager, int maxNumOutputChannels, std::function<void()> saveSettings)
        : deviceManager (manager), onSettingsChanged (std::move (saveSettings)), midiCard (std::move (actionSender))
    {
        addAndMakeVisible (midiCard);
        if (deviceManager != nullptr)
        {
            selector = std::make_unique<juce::AudioDeviceSelectorComponent> (*deviceManager,
                                                                             0, 0,  // No audio inputs (SOURCE is an instrument)
                                                                             1, juce::jmax (2, maxNumOutputChannels),
                                                                             true,   // MIDI inputs (with the "Bluetooth MIDI" button where available)
                                                                             false,  // No MIDI output selector
                                                                             true,   // Channels as stereo pairs
                                                                             false); // Hide the advanced options
            addAndMakeVisible (deviceCard);
            viewport.setViewedComponent (selector.get(), false);
            viewport.setScrollBarsShown (true, false);
            deviceCard.addAndMakeVisible (viewport);
            deviceManager->addChangeListener (this);
        }
    }

    ~SettingsPanel() override
    {
        if (deviceManager != nullptr)
            deviceManager->removeChangeListener (this);
    }

    void updateFromState (const juce::ValueTree& state) { midiCard.updateFromState (state); }

   #if SOURCE_LINK
    /** Shows the Ableton Link settings below the other settings. */
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
        // A centred column (the device selector is laid out for widths around 500 px)
        auto column = getLocalBounds().withSizeKeepingCentre (juce::jmin (getWidth(), 640), getHeight());
        midiCard.setBounds (column.removeFromTop (MidiSettingsCard::preferredHeight));
        column.removeFromTop (8);
       #if SOURCE_LINK
        if (linkCard != nullptr)
        {
            linkCard->setBounds (column.removeFromBottom (juce::jmin (LinkSettingsCard::preferredHeight, column.getHeight() / 2)));
            column.removeFromBottom (8);
        }
       #endif
        if (selector != nullptr)
        {
            deviceCard.setBounds (column);
            auto content = deviceCard.getContentBounds();
            viewport.setBounds (content);
            selector->setSize (content.getWidth() - viewport.getScrollBarThickness(), juce::jmax (content.getHeight(), 380));
        }
    }

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override
    {
        if (onSettingsChanged != nullptr)
            onSettingsChanged();
    }

    juce::AudioDeviceManager* deviceManager = nullptr;
    std::function<void()> onSettingsChanged;
    MidiSettingsCard midiCard;
    CardComponent deviceCard { "Audio & MIDI devices" };
    juce::Viewport viewport;
    std::unique_ptr<juce::AudioDeviceSelectorComponent> selector;
   #if SOURCE_LINK
    std::unique_ptr<LinkSettingsCard> linkCard;
   #endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SettingsPanel)
};
