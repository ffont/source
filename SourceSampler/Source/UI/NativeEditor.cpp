/*
  ==============================================================================

    NativeEditor.cpp

  ==============================================================================
*/

#include "NativeEditor.h"
#include "SourceUIBinaryData.h"


//==============================================================================
void LevelMetersComponent::setLevels (const std::vector<float>& newLevels)
{
    if (newLevels == levels)
        return;
    levels = newLevels;
    repaint();
}

void LevelMetersComponent::paint (juce::Graphics& g)
{
    // Vertical bars, or horizontal bars (one per row) if the component is wider than tall
    const int numChannels = juce::jmax (1, (int) levels.size());
    const bool horizontal = getWidth() > getHeight();
    const int gap = (horizontal || getWidth() < 30) ? 2 : 6;
    const int along = horizontal ? getHeight() : getWidth();
    const int meterSize = juce::jmax (1, juce::jmin (30, (along - (numChannels - 1) * gap) / numChannels));
    auto b = getLocalBounds();
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto meter = (horizontal ? b.removeFromTop (meterSize) : b.removeFromLeft (meterSize)).toFloat();
        horizontal ? b.removeFromTop (gap) : b.removeFromLeft (gap);
        g.setColour (SourceColours::control);
        g.drawRect (meter, 1.0f);
        const float level = ch < (int) levels.size() ? levels[(size_t) ch] : 0.0f;
        // Map -60..0 dB to 0..1
        const float db = juce::Decibels::gainToDecibels (level, -60.0f);
        const float proportion = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
        g.setColour (db > -1.0f ? SourceColours::warning : SourceColours::accent);
        auto inner = meter.reduced (1.0f);
        g.fillRect (horizontal ? inner.removeFromLeft (inner.getWidth() * proportion) : inner.removeFromBottom (inner.getHeight() * proportion));
    }
}

//==============================================================================
void VoicesComponent::setVoices (const std::vector<SourceSampler::VoiceInfo>& voices)
{
    std::vector<bool> newActive;
    for (const auto& v : voices)
        newActive.push_back (v.active);
    if (newActive == activeVoices)
        return;
    activeVoices = newActive;
    repaint();
}

void VoicesComponent::paint (juce::Graphics& g)
{
    // Squares centred in the component: one row for up to 16 voices, otherwise two rows of (almost) the same length.
    // Squares are made smaller if they don't fit.
    const int numVoices = (int) activeVoices.size();
    if (numVoices == 0)
        return;
    const int numRows = numVoices > 16 ? 2 : 1;
    const int perRow = (numVoices + numRows - 1) / numRows;
    const int gap = 3;
    const int size = juce::jlimit (4, 14, juce::jmin ((getWidth() + gap) / perRow - gap, (getHeight() + gap) / numRows - gap));
    const int step = size + gap;
    const int x0 = (getWidth() - (perRow * step - 3)) / 2;
    const int y0 = (getHeight() - (numRows * step - 3)) / 2;
    for (size_t i = 0; i < activeVoices.size(); ++i)
    {
        const int x = x0 + (int) (i % (size_t) perRow) * step;
        const int y = y0 + (int) (i / (size_t) perRow) * step;
        auto r = juce::Rectangle<int> (x, y, size, size).toFloat();
        g.setColour (activeVoices[i] ? SourceColours::accent : SourceColours::background);
        g.fillRoundedRectangle (r, 2.0f);
        g.setColour (SourceColours::control);
        g.drawRoundedRectangle (r, 2.0f, 1.0f);
    }
}


//==============================================================================
static void setupLabel (juce::Label& label, const juce::String& text, juce::Component& parent)
{
    label.setText (text, juce::dontSendNotification);
    label.setFont (juce::Font (juce::FontOptions (12.0f)));
    label.setColour (juce::Label::textColourId, SourceColours::dimText);
    parent.addAndMakeVisible (label);
}

static void setupNumberSlider (juce::Slider& slider, double min, double max, double interval, double value, juce::Component& parent)
{
    slider.setSliderStyle (juce::Slider::IncDecButtons);
    slider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 44, 20);
    slider.setRange (min, max, interval);
    slider.setValue (value, juce::dontSendNotification);
    slider.setIncDecButtonsMode (juce::Slider::incDecButtonsDraggable_Vertical);
    parent.addAndMakeVisible (slider);
}


/** Returns the "SOURCE" lettering of the title image used by the web UI: the black badge is made transparent and the
    image is cropped to the first block of lettering (the "A Freesound Community Sampler" subtitle below is dropped). */
static juce::Image makeTitleImage()
{
    auto source = juce::ImageCache::getFromMemory (SourceUIBinaryData::source_title_png, SourceUIBinaryData::source_title_pngSize);
    if (! source.isValid())
        return {};

    juce::Image image = source.convertedToFormat (juce::Image::ARGB);
    image.duplicateIfShared();
    const auto isLettering = [] (juce::Colour c) { return c.getAlpha() > 128 && c.getBrightness() > 0.16f; };

    int minX = image.getWidth(), maxX = -1, firstRow = -1, lastRow = -1;
    {
        juce::Image::BitmapData pixels (image, juce::Image::BitmapData::readWrite);
        for (int y = 0; y < image.getHeight(); ++y)
        {
            bool rowHasLettering = false;
            for (int x = 0; x < image.getWidth(); ++x)
            {
                const auto c = pixels.getPixelColour (x, y);
                if (isLettering (c))
                {
                    rowHasLettering = true;
                    if (lastRow < 0 || lastRow == y - 1 || firstRow < 0)
                    {
                        minX = juce::jmin (minX, x);
                        maxX = juce::jmax (maxX, x);
                    }
                }
                else if (c.getBrightness() < 0.05f)
                {
                    pixels.setPixelColour (x, y, juce::Colours::transparentBlack);
                }
            }

            if (rowHasLettering && (firstRow < 0 || lastRow == y - 1))
            {
                if (firstRow < 0)
                    firstRow = y;
                lastRow = y;
            }
            else if (! rowHasLettering && firstRow >= 0)
            {
                break;  // End of the first block of lettering
            }
        }
    }
    if (firstRow < 0)
        return image;
    return image.getClippedImage (juce::Rectangle<int>::leftTopRightBottom (minX, firstRow, maxX + 1, lastRow + 1));
}

SourceNativeEditor::SourceNativeEditor (SourceSamplerAudioProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p),
      sourceSampler (p.source),
      actions { [this] (const juce::String& message) { sourceSampler.actionListenerCallback (message); } },
      soundList (actions),
      soundEditor (p.source, actions),
      searchPanel (p.source.freesoundSearch, actions, [this] { return getPresetState(); }),
      fxPanel (actions),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (content);

    // Header
    titleImage.setImage (makeTitleImage());
    titleImage.setImagePlacement (juce::RectanglePlacement::xLeft | juce::RectanglePlacement::yMid | juce::RectanglePlacement::onlyReduceInSize);
    content.addAndMakeVisible (titleImage);
    statusLabel.setColour (juce::Label::textColourId, SourceColours::dimText);
    statusLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    content.addAndMakeVisible (voicesComponent);
    levelMeters.setTooltip ("Output level");
    content.addAndMakeVisible (levelMeters);
    muteButton.setClickingTogglesState (true);
    muteButton.setToggleState (sourceSampler.isOutputMuted(), juce::dontSendNotification);
    muteButton.setIcon (sourceSampler.isOutputMuted() ? IconButton::Icon::speakerMuted : IconButton::Icon::speaker);
    muteButton.onClick = [this]
    {
        const bool muted = muteButton.getToggleState();
        sourceSampler.setOutputMuted (muted);
        muteButton.setIcon (muted ? IconButton::Icon::speakerMuted : IconButton::Icon::speaker);
        muteButton.setTooltip (muted ? "Unmute output" : "Mute output");
    };
    content.addAndMakeVisible (muteButton);
    logoImage.setImage (juce::ImageCache::getFromMemory (SourceUIBinaryData::rita_aurora_hens_png, SourceUIBinaryData::rita_aurora_hens_pngSize));
    logoImage.setImagePlacement (juce::RectanglePlacement::xLeft | juce::RectanglePlacement::yMid | juce::RectanglePlacement::onlyReduceInSize);
    logoImage.setTooltip ("Rita & Aurora");
    content.addAndMakeVisible (logoImage);

    // Tabs
    int tabIndex = 0;
    for (auto* button : { &generalTabButton, &searchTabButton, &fxTabButton })
    {
        button->setRadioGroupId (1);
        button->setClickingTogglesState (true);
        button->setColour (juce::TextButton::buttonOnColourId, SourceColours::accent.darker (0.3f));
        button->setConnectedEdges ((tabIndex > 0 ? juce::Button::ConnectedOnLeft : 0) | (tabIndex < 2 ? juce::Button::ConnectedOnRight : 0));
        content.addAndMakeVisible (button);
        ++tabIndex;
    }
    generalTabButton.onClick = [this] { showTab (Tab::general); };
    searchTabButton.onClick = [this] { showTab (Tab::search); };
    fxTabButton.onClick = [this] { showTab (Tab::fx); };
    content.addChildComponent (searchPanel);
    content.addChildComponent (fxPanel);

    // Preset & settings card
    content.addAndMakeVisible (presetCard);
    // Preset slots: the dropdown lists all slots (selecting one loads it), the number of the current slot is shown
    // next to its (editable) name, "Save" saves to the current slot and "Save to..." to another one
    presetSlotsCombo.setTooltip ("Load a preset (empty slots load an empty preset)");
    presetSlotsCombo.onPopupOpening = [this] { refreshPresetSlots(); };
    presetSlotsCombo.onChange = [this]
    {
        const int index = presetSlotsCombo.getSelectedId() - 1;
        if (index >= 0 && index != (int) sourceSampler.state.getProperty (SourceIDs::currentPresetIndex, -1))
            sendAction (ACTION_LOAD_PRESET, { juce::String (index) });
    };
    presetCard.addAndMakeVisible (presetSlotsCombo);
    presetNameEditor.setTextToShowWhenEmpty ("Preset name", SourceColours::dimText);
    presetNameEditor.onReturnKey = [this] { savePresetButton.triggerClick(); };
    presetCard.addAndMakeVisible (presetNameEditor);
    presetCard.addAndMakeVisible (savePresetButton);
    presetCard.addAndMakeVisible (saveToPresetButton);
    presetCard.addAndMakeVisible (previousPresetButton);
    presetCard.addAndMakeVisible (nextPresetButton);
    savePresetButton.setTooltip ("Save the preset (with this name) to its slot");
    savePresetButton.onClick = [this] { savePresetTo (juce::jmax (0, (int) sourceSampler.state.getProperty (SourceIDs::currentPresetIndex, 0))); };
    saveToPresetButton.setTooltip ("Save the preset to another slot");
    saveToPresetButton.onClick = [this] { showSaveToMenu(); };
    refreshPresetSlots();
    previousPresetButton.onClick = [this]
    {
        const int index = juce::jmax (0, (int) sourceSampler.state.getProperty (SourceIDs::currentPresetIndex, 0) - 1);
        sendAction (ACTION_LOAD_PRESET, { juce::String (index) });
    };
    nextPresetButton.onClick = [this]
    {
        const int index = juce::jmin (NUM_PRESET_SLOTS - 1, (int) sourceSampler.state.getProperty (SourceIDs::currentPresetIndex, -1) + 1);
        sendAction (ACTION_LOAD_PRESET, { juce::String (index) });
    };

    setupLabel (midiChannelLabel, "MIDI in channel", presetCard);
    midiChannelCombo.addItem ("All", 1);
    for (int i = 1; i <= 16; ++i)
        midiChannelCombo.addItem (juce::String (i), i + 1);
    midiChannelCombo.onChange = [this] { sendAction (ACTION_SET_GLOBAL_MIDI_IN_CHANNEL, { juce::String (midiChannelCombo.getSelectedId() - 1) }); };
    presetCard.addAndMakeVisible (midiChannelCombo);

    setupLabel (numVoicesLabel, "Num voices", presetCard);
    setupNumberSlider (numVoicesSlider, 1, SourceSamplerSynthesiser::maxNumVoices, 1, SourceDefaults::numVoices, presetCard);
    numVoicesSlider.onValueChange = [this] { sendAction (ACTION_SET_POLYPHONY, { juce::String ((int) numVoicesSlider.getValue()) }); };

    // MPE input settings
    mpeToggle.setTooltip ("Enable MPE input (lower zone: master channel 1, notes on channels 2-16). When enabled, all sounds respond "
                          "to all channels and pitch bend, pressure and slide (CC74) on a note's channel only affect that note.");
    auto sendMpeSettings = [this]
    {
        sendAction (ACTION_SET_MPE_SETTINGS, { mpeToggle.getToggleState() ? "1" : "0", juce::String ((int) mpeBendRangeSlider.getValue()) });
    };
    mpeToggle.onClick = sendMpeSettings;
    presetCard.addAndMakeVisible (mpeToggle);
    mpeToggle.setButtonText ("MPE input");
    setupLabel (mpeBendLabel, "Bend range", presetCard);
    setupNumberSlider (mpeBendRangeSlider, 1, 96, 1, SourceDefaults::mpePitchBendRange, presetCard);
    mpeBendRangeSlider.setTooltip ("MPE per-note pitch bend range (semitones)");
    mpeBendRangeSlider.onValueChange = sendMpeSettings;

    // Get new sounds card
    content.addAndMakeVisible (querySoundsCard);
    setupLabel (queryLabel, "Query", querySoundsCard);
    queryEditor.setTextToShowWhenEmpty ("percussion", SourceColours::dimText);
    queryEditor.onReturnKey = [this] { runQuery (ACTION_REPLACE_EXISTING_SOUNDS_FROM_QUERY); };
    querySoundsCard.addAndMakeVisible (queryEditor);
    setupLabel (numSoundsLabel, "Num sounds", querySoundsCard);
    setupNumberSlider (numSoundsSlider, 1, 128, 1, 16, querySoundsCard);
    setupLabel (lengthLabel, "Sound length", querySoundsCard);
    setupNumberSlider (minLengthSlider, 0.0, 300.0, 0.1, 0.0, querySoundsCard);
    setupLabel (lengthToLabel, "to", querySoundsCard);
    lengthToLabel.setJustificationType (juce::Justification::centred);
    setupNumberSlider (maxLengthSlider, 0.0, 300.0, 0.1, 0.5, querySoundsCard);
    setupLabel (layoutLabel, "Note layout", querySoundsCard);
    noteLayoutCombo.addItem ("Contiguous", NOTE_MAPPING_TYPE_CONTIGUOUS + 1);
    noteLayoutCombo.addItem ("Interleaved", NOTE_MAPPING_TYPE_INTERLEAVED + 1);
    noteLayoutCombo.addItem ("All", NOTE_MAPPING_TYPE_ALL + 1);
    noteLayoutCombo.onChange = [this] { sendAction (ACTION_REAPPLY_LAYOUT, { juce::String (noteLayoutCombo.getSelectedId() - 1) }); };
    querySoundsCard.addAndMakeVisible (noteLayoutCombo);
    replaceAllButton.onClick = [this] { runQuery (ACTION_REPLACE_EXISTING_SOUNDS_FROM_QUERY); };
    replaceOneButton.onClick = [this] { runQuery (ACTION_REPLACE_SOUND_FROM_QUERY); };
    addSoundsButton.onClick = [this] { runQuery (ACTION_ADD_NEW_SOUNDS_FROM_QUERY); };
    querySoundsCard.addAndMakeVisible (replaceAllButton);
    querySoundsCard.addAndMakeVisible (replaceOneButton);
    querySoundsCard.addAndMakeVisible (addSoundsButton);
    querySoundsCard.addAndMakeVisible (statusLabel);
    setupNumberSlider (soundToReplaceSlider, 1, 128, 1, 1, querySoundsCard);

    // Sounds
    soundList.onSelectionChanged = [this] (const juce::String& uuid) { soundEditor.setSound (uuid); };
    content.addAndMakeVisible (soundList);
    content.addAndMakeVisible (soundEditor);

    // Keyboard
    keyboard.setAvailableRange (21, 108);
    keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xffe8e8e8));
    keyboard.setColour (juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour (0xff202020));
    keyboard.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour (0xff606060));
    content.addAndMakeVisible (keyboard);

    sourceSampler.state.addListener (this);
    updateFromState();
    showTab (Tab::general);

    setResizable (true, true);
   #if JUCE_IOS
    setResizeLimits (300, 300, 4000, 4000);  // The UI is scaled to fit whatever size the app/host gives us
   #else
    setResizeLimits (800, 560, 2400, 1600);
   #endif
    setSize (1180, 820);
    startTimerHz (30);

   #if JUCE_DEBUG
    // Development helpers (debug builds only):
    //  SOURCE_DEBUG_ADD_FILE=<audio file>  adds a local audio file as a sound when the editor opens
    //  SOURCE_DEBUG_SNAPSHOT=<png file>    renders the editor to a PNG file a few seconds after opening (no screen access needed)
    const auto debugFile = juce::SystemStats::getEnvironmentVariable ("SOURCE_DEBUG_ADD_FILE", {});
    if (debugFile.isNotEmpty())
        juce::Timer::callAfterDelay (500, [safe = juce::Component::SafePointer<SourceNativeEditor> (this), debugFile]
        {
            if (safe != nullptr)
                safe->filesDropped ({ debugFile }, 0, 0);
        });
    //  SOURCE_DEBUG_LOAD_PRESET=<index>    loads that preset when the editor opens
    const auto debugPreset = juce::SystemStats::getEnvironmentVariable ("SOURCE_DEBUG_LOAD_PRESET", {});
    if (debugPreset.isNotEmpty())
        juce::Timer::callAfterDelay (500, [safe = juce::Component::SafePointer<SourceNativeEditor> (this), debugPreset]
        {
            if (safe != nullptr)
                safe->sendAction (ACTION_LOAD_PRESET, { debugPreset });
        });
    //  SOURCE_DEBUG_TAB=general|search|fx   opens that tab
    //  SOURCE_DEBUG_SEARCH=<query>         runs a search in the Search tab (SOURCE_DEBUG_SEARCH_VIEW=list shows the list)
    const auto debugTab = juce::SystemStats::getEnvironmentVariable ("SOURCE_DEBUG_TAB", {});
    if (debugTab == "search") showTab (Tab::search);
    if (debugTab == "fx") showTab (Tab::fx);
    const auto debugSearch = juce::SystemStats::getEnvironmentVariable ("SOURCE_DEBUG_SEARCH", {});
    if (debugSearch.isNotEmpty())
    {
        showTab (Tab::search);
        FreesoundSearchOptions options;
        options.query = debugSearch;
        options.maxDuration = 10.0f;
        sourceSampler.freesoundSearch.search (options);
        if (juce::SystemStats::getEnvironmentVariable ("SOURCE_DEBUG_SEARCH_VIEW", {}) == "list")
            searchPanel.showMap (false);
    }
    const auto snapshotPath = juce::SystemStats::getEnvironmentVariable ("SOURCE_DEBUG_SNAPSHOT", {});
    if (snapshotPath.isNotEmpty())
        juce::Timer::callAfterDelay (juce::jmax (500, juce::SystemStats::getEnvironmentVariable ("SOURCE_DEBUG_SNAPSHOT_DELAY", "3500").getIntValue()), [safe = juce::Component::SafePointer<SourceNativeEditor> (this), snapshotPath]
        {
            if (safe == nullptr)
                return;
            if (safe->soundList.getNumSounds() > 0)
                safe->soundList.selectSound (safe->soundList.getItems().front().uuid);
            if (safe->currentTab == Tab::search)
                safe->searchPanel.selectResult (0, false);
            // SOURCE_DEBUG_SNAPSHOT_SIZE=<width>x<height> renders the editor at that size (e.g. to preview iPad layouts)
            const auto size = juce::StringArray::fromTokens (juce::SystemStats::getEnvironmentVariable ("SOURCE_DEBUG_SNAPSHOT_SIZE", {}), "x", {});
            if (size.size() == 2)
            {
                safe->setResizeLimits (100, 100, 5000, 5000);
                safe->setSize (size[0].getIntValue(), size[1].getIntValue());
            }
            safe->updateFromState();
            juce::Timer::callAfterDelay (500, [safe, snapshotPath]
            {
                if (safe == nullptr)
                    return;
                auto image = safe->createComponentSnapshot (safe->getLocalBounds(), true, 1.0f);
                juce::File file (snapshotPath);
                file.deleteFile();
                juce::FileOutputStream stream (file);
                juce::PNGImageFormat().writeImageToStream (image, stream);
                // Also save the whole (scrollable) parameters panel of the sound editor
                juce::File paramsFile = file.getSiblingFile (file.getFileNameWithoutExtension() + "-params.png");
                paramsFile.deleteFile();
                juce::FileOutputStream paramsStream (paramsFile);
                juce::PNGImageFormat().writeImageToStream (safe->soundEditor.createParametersSnapshot(), paramsStream);
                DBG ("Saved editor snapshot to " << file.getFullPathName());
            });
        });
   #endif
}

SourceNativeEditor::~SourceNativeEditor()
{
    stopTimer();
    sourceSampler.state.removeListener (this);
    setLookAndFeel (nullptr);
}

void SourceNativeEditor::refreshPresetSlots()
{
    // Checks which slots have a preset file and reads the names of the files that changed since the last refresh
    bool changed = false;
    for (int i = 0; i < NUM_PRESET_SLOTS; ++i)
    {
        const auto file = sourceSampler.presetFilesLocation.getChildFile (juce::String (i) + ".xml");
        const bool used = file.existsAsFile();
        const auto modificationTime = used ? file.getLastModificationTime() : juce::Time();
        if (used == presetSlotUsed[(size_t) i] && modificationTime == presetSlotModificationTimes[(size_t) i])
            continue;
        presetSlotUsed[(size_t) i] = used;
        presetSlotModificationTimes[(size_t) i] = modificationTime;
        presetSlotNames[(size_t) i] = used ? SourceHelpers::readPresetNameFromFile (file) : juce::String();
        changed = true;
    }
    if (! changed && presetSlotsCombo.getNumItems() == NUM_PRESET_SLOTS)
        return;

    const int selectedId = presetSlotsCombo.getSelectedId();
    presetSlotsCombo.clear (juce::dontSendNotification);
    for (int i = 0; i < NUM_PRESET_SLOTS; ++i)
        presetSlotsCombo.addItem (getPresetSlotText (i), i + 1);
    presetSlotsCombo.setSelectedId (selectedId, juce::dontSendNotification);
}

juce::String SourceNativeEditor::getPresetSlotText (int index) const
{
    juce::String text = juce::String (index).paddedLeft ('0', 3) + "   ";
    if (! presetSlotUsed[(size_t) index])
        return text + "----";
    const auto& name = presetSlotNames[(size_t) index];
    return text + (name.isNotEmpty() ? name : juce::String ("(no name)"));
}

void SourceNativeEditor::savePresetTo (int index)
{
    sendAction (ACTION_SAVE_PRESET, { SourceActionSender::sanitize (presetNameEditor.getText().trim()), juce::String (index) });
    presetNameEditor.unfocusAllComponents();
    refreshPresetSlots();
}

void SourceNativeEditor::showSaveToMenu()
{
    refreshPresetSlots();
    const int currentIndex = sourceSampler.state.getProperty (SourceIDs::currentPresetIndex, -1);
    juce::PopupMenu menu;
    menu.addSectionHeader ("Save preset to slot");
    for (int i = 0; i < NUM_PRESET_SLOTS; ++i)
        menu.addItem (i + 1, getPresetSlotText (i), true, i == currentIndex);

    juce::Component::SafePointer<SourceNativeEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&saveToPresetButton).withMaximumNumColumns (4),
                        [safe, currentIndex] (int result)
    {
        if (safe == nullptr || result <= 0)
            return;
        const int index = result - 1;
        if (index == currentIndex || ! safe->presetSlotUsed[(size_t) index])
        {
            safe->savePresetTo (index);
            return;
        }
        // Ask before replacing another preset
        const auto message = "Slot " + juce::String (index) + " has the preset \"" + safe->presetSlotNames[(size_t) index]
                             + "\". Do you want to replace it?";
        juce::AlertWindow::showAsync (juce::MessageBoxOptions::makeOptionsOkCancel (juce::MessageBoxIconType::WarningIcon, "Replace preset?",
                                                                            message, "Replace", "Cancel", safe.getComponent()),
                                      [safe, index] (int choice)
                                      {
                                          if (safe != nullptr && choice == 1)
                                              safe->savePresetTo (index);
                                      });
    });
}

void SourceNativeEditor::sendAction (const juce::String& actionName, const juce::StringArray& parameters)
{
    actions.send (actionName, parameters);
}

void SourceNativeEditor::showTab (Tab tab)
{
    currentTab = tab;
    const bool general = tab == Tab::general;
    for (auto* c : std::initializer_list<juce::Component*> { &presetCard, &querySoundsCard, &soundList, &soundEditor })
        c->setVisible (general);
    searchPanel.setVisible (tab == Tab::search);
    fxPanel.setVisible (tab == Tab::fx);
    generalTabButton.setToggleState (general, juce::dontSendNotification);
    searchTabButton.setToggleState (tab == Tab::search, juce::dontSendNotification);
    fxTabButton.setToggleState (tab == Tab::fx, juce::dontSendNotification);
}

void SourceNativeEditor::runQuery (const juce::String& queryActionName)
{
    const juce::String query = SourceActionSender::sanitize (queryEditor.getText().trim());
    const juce::String minLength (minLengthSlider.getValue());
    const juce::String maxLength (maxLengthSlider.getValue());
    const juce::String layout (noteLayoutCombo.getSelectedId() > 0 ? noteLayoutCombo.getSelectedId() - 1 : SourceDefaults::noteLayoutType);
    if (queryActionName == ACTION_REPLACE_SOUND_FROM_QUERY)
    {
        const int index = (int) soundToReplaceSlider.getValue() - 1;
        const auto& items = soundList.getItems();
        if (index < 0 || index >= (int) items.size())
        {
            statusLabel.setText ("There is no sound #" + juce::String (index + 1) + " to replace", juce::dontSendNotification);
            return;
        }
        sendAction (queryActionName, { items[(size_t) index].uuid, query, minLength, maxLength, layout });
    }
    else
    {
        sendAction (queryActionName, { query, juce::String ((int) numSoundsSlider.getValue()), minLength, maxLength, layout });
    }
}

void SourceNativeEditor::updateFromState()
{
    stateDirty = false;
    const auto& state = sourceSampler.state;
    const auto preset = getPresetState();

    const int presetIndex = state.getProperty (SourceIDs::currentPresetIndex, -1);
    const auto presetName = preset.getProperty (SourceIDs::name).toString();
    if (presetIndex != lastPresetIndexShown || presetName != lastPresetNameShown)
    {
        // A preset was loaded or saved: refresh the slots (only the files that changed are read again)
        lastPresetIndexShown = presetIndex;
        lastPresetNameShown = presetName;
        refreshPresetSlots();
        if (! presetNameEditor.hasKeyboardFocus (true))
            presetNameEditor.setText (presetName, false);
    }
    if (! presetSlotsCombo.isPopupActive())
        presetSlotsCombo.setSelectedId (presetIndex >= 0 ? presetIndex + 1 : 0, juce::dontSendNotification);
    midiChannelCombo.setSelectedId ((int) state.getProperty (SourceIDs::globalMidiInChannel, 0) + 1, juce::dontSendNotification);
    const bool mpeEnabled = state.getProperty (SourceIDs::mpeEnabled, SourceDefaults::mpeEnabled);
    mpeToggle.setToggleState (mpeEnabled, juce::dontSendNotification);
    if (! mpeBendRangeSlider.isMouseButtonDown())
        mpeBendRangeSlider.setValue ((int) state.getProperty (SourceIDs::mpePitchBendRange, SourceDefaults::mpePitchBendRange), juce::dontSendNotification);
    mpeBendRangeSlider.setEnabled (mpeEnabled);
    mpeBendLabel.setEnabled (mpeEnabled);
    if (! numVoicesSlider.isMouseButtonDown())
        numVoicesSlider.setValue ((int) preset.getProperty (SourceIDs::numVoices, SourceDefaults::numVoices), juce::dontSendNotification);
    if (! noteLayoutCombo.isPopupActive())
        noteLayoutCombo.setSelectedId ((int) preset.getProperty (SourceIDs::noteLayoutType, SourceDefaults::noteLayoutType) + 1, juce::dontSendNotification);
    fxPanel.updateFromState (preset);
    searchPanel.updateFromState (preset);

    soundList.updateFromState (preset);
    if (soundEditor.getSoundUUID() != soundList.getSelectedSoundUUID())
        soundEditor.setSound (soundList.getSelectedSoundUUID());
    else
        soundEditor.updateFromState();
}

void SourceNativeEditor::timerCallback()
{
    if (stateDirty)
        updateFromState();

    // Volatile state
    const auto voices = sourceSampler.getVoicesInfo();
    voicesComponent.setVoices (voices);
    std::map<juce::String, int> activeVoicesPerSound;
    std::vector<float> playheads;
    for (const auto& voice : voices)
    {
        if (! voice.active)
            continue;
        activeVoicesPerSound[voice.soundUUID] += 1;
        if (voice.soundUUID == soundEditor.getSoundUUID())
            playheads.push_back (voice.playPosition);
    }
    soundList.setActiveVoices (activeVoicesPerSound);
    soundEditor.setPlayheads (playheads);
    levelMeters.setLevels (sourceSampler.getOutputLevels());
    searchPanel.setPreviewState ((int) sourceSampler.getPreviewState());

    juce::String status;
    if (sourceSampler.isQueryingSounds())
    {
        status = "Searching Freesound...";
    }
    else
    {
        const int numResults = sourceSampler.state.getProperty (SourceIDs::numResultsLastQuery, -2);
        if (numResults >= 0)
            status << numResults << " results in last query";
    }
    if (statusLabel.getText() != status)
        statusLabel.setText (status, juce::dontSendNotification);
}

bool SourceNativeEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& file : files)
        if (SourceSound::fileLocationIsSupportedAudioFileFormat (juce::File (file)))
            return true;
    return false;
}

void SourceNativeEditor::filesDropped (const juce::StringArray& files, int, int)
{
    isDraggingFiles = false;
    repaint();
    int numAdded = 0;
    for (const auto& path : files)
    {
        juce::File file (path);
        if (! SourceSound::fileLocationIsSupportedAudioFileFormat (file))
            continue;
        // Parameters of ACTION_ADD_SOUND: soundID, name, user, license, previewURL, localFilePath, format, sizeBytes, slices, midiNotes, midiRootNote
        sendAction (ACTION_ADD_SOUND, { "-1", SourceActionSender::sanitize (file.getFileNameWithoutExtension()), "", "", "",
                                        file.getFullPathName(), file.getFileExtension().removeCharacters ("."),
                                        juce::String (file.getSize()), "", "", "-1" });
        numAdded += 1;
    }
    if (numAdded > 0)
    {
        // Lay out notes for all sounds (including the new ones)
        const int layout = getPresetState().getProperty (SourceIDs::noteLayoutType, SourceDefaults::noteLayoutType);
        sendAction (ACTION_REAPPLY_LAYOUT, { juce::String (layout) });
    }
}

void SourceNativeEditor::paint (juce::Graphics& g)
{
    g.fillAll (SourceColours::background);
}

void SourceNativeEditor::paintOverChildren (juce::Graphics& g)
{
    if (isDraggingFiles)
    {
        g.setColour (SourceColours::accent.withAlpha (0.15f));
        g.fillRect (getLocalBounds());
        g.setColour (SourceColours::accent);
        g.drawRect (getLocalBounds(), 3);
        g.setFont (juce::Font (juce::FontOptions (20.0f, juce::Font::bold)));
        g.drawText ("Drop audio files to add them as sounds", getLocalBounds(), juce::Justification::centred);
    }
}

void SourceNativeEditor::resized()
{
    // Lay out the UI at (at least) a minimum logical size, and scale it down if the available area is smaller
    const auto area = getLocalBounds();
    const bool portrait = area.getHeight() > area.getWidth();
    const float minWidth = portrait ? 740.0f : 1160.0f;
    const float minHeight = portrait ? 980.0f : 740.0f;
    const float scale = juce::jmin (1.0f, (float) area.getWidth() / minWidth, (float) area.getHeight() / minHeight);
    content.setTransform (juce::AffineTransform::scale (scale));
    content.setBounds (0, 0, juce::roundToInt ((float) area.getWidth() / scale), juce::roundToInt ((float) area.getHeight() / scale));
    layoutContent (content.getLocalBounds(), portrait);
}

void SourceNativeEditor::layoutContent (juce::Rectangle<int> bounds, bool portrait)
{
    auto b = bounds.reduced (8);

    // Header: logo and title on the left, tabs on the right, voices and output meters in the centre
    auto header = b.removeFromTop (44);
    const auto& logo = logoImage.getImage();
    const float logoAspect = logo.isValid() ? (float) logo.getWidth() / (float) logo.getHeight() : 1.0f;
    logoImage.setBounds (header.removeFromLeft (juce::roundToInt ((float) header.getHeight() * logoAspect)));
    header.removeFromLeft (8);
    const auto& title = titleImage.getImage();
    const float titleAspect = title.isValid() ? (float) title.getWidth() / (float) title.getHeight() : 3.0f;
    titleImage.setBounds (header.removeFromLeft (juce::roundToInt ((float) header.getHeight() * titleAspect)).reduced (0, 2));
    // Right side: tabs, output meters (vertical) and mute button
    muteButton.setBounds (header.removeFromRight (30).withSizeKeepingCentre (30, 30));
    header.removeFromRight (8);
    levelMeters.setBounds (header.removeFromRight (16).withSizeKeepingCentre (16, 36));
    header.removeFromRight (10);
    auto tabs = header.removeFromRight (3 * 84).withSizeKeepingCentre (3 * 84, 28);
    for (auto* button : { &generalTabButton, &searchTabButton, &fxTabButton })
        button->setBounds (tabs.removeFromLeft (84));
    const int voicesWidth = juce::jmin (header.getWidth() / 2, SourceSamplerSynthesiser::maxNumVoices * 17);
    voicesComponent.setBounds (juce::Rectangle<int> (voicesWidth, header.getHeight()).withCentre (bounds.getCentre().withY (header.getCentreY())));
    b.removeFromTop (8);

   #if JUCE_IOS
    const int keyboardHeight = 96;  // Bigger keys for touch
   #else
    const int keyboardHeight = 64;
   #endif
    keyboard.setBounds (b.removeFromBottom (keyboardHeight));
    {
        // Show as many keys as fit with a comfortable key width (bigger on touch screens), starting from C1
       #if JUCE_IOS
        const float targetKeyWidth = 30.0f;
       #else
        const float targetKeyWidth = 18.0f;
       #endif
        const int numWhiteKeys = juce::jlimit (15, 52, juce::roundToInt ((float) keyboard.getWidth() / targetKeyWidth));
        keyboard.setKeyWidth ((float) keyboard.getWidth() / (float) numWhiteKeys);
        keyboard.setLowestVisibleKey (numWhiteKeys > 36 ? 24 : 36);
    }
    b.removeFromBottom (8);

    // The other tabs use all the space between the header and the keyboard
    searchPanel.setBounds (b);
    fxPanel.setBounds (b);

    // General tab. Landscape: preset and "get new sounds" cards and the list of sounds on the left, the sound editor
    // (full height) on the right. Portrait: the two cards side by side, then the list of sounds and the sound editor.
    const int gap = 8;
    const int presetCardHeight = 26 + 16 + 5 * 27 - 5;
    const int queryCardHeight = 236;
    juce::Rectangle<int> listArea, editorArea;
    if (portrait)
    {
        auto top = b.removeFromTop (queryCardHeight);
        presetCard.setBounds (top.removeFromLeft ((top.getWidth() - gap) / 2).withHeight (presetCardHeight));
        top.removeFromLeft (gap);
        querySoundsCard.setBounds (top);
        b.removeFromTop (gap);
        listArea = b.removeFromTop (juce::jmin (240, b.getHeight() / 3));
        b.removeFromTop (gap);
        editorArea = b;
    }
    else
    {
        auto left = b.removeFromLeft (juce::jlimit (380, 460, b.getWidth() / 3));
        b.removeFromLeft (gap);
        editorArea = b;
        presetCard.setBounds (left.removeFromTop (presetCardHeight));
        left.removeFromTop (gap);
        querySoundsCard.setBounds (left.removeFromTop (queryCardHeight));
        left.removeFromTop (gap);
        listArea = left;
    }
    soundList.setBounds (listArea);
    soundEditor.setBounds (editorArea);

    const int rowHeight = 22;
    const int rowGap = 5;
    const int labelWidth = 100;

    // Preset card content
    {
        auto c = presetCard.getContentBounds();
        auto row = c.removeFromTop (rowHeight);
        nextPresetButton.setBounds (row.removeFromRight (24));
        row.removeFromRight (2);
        previousPresetButton.setBounds (row.removeFromRight (24));
        row.removeFromRight (6);
        presetSlotsCombo.setBounds (row);
        c.removeFromTop (rowGap);
        row = c.removeFromTop (rowHeight);
        saveToPresetButton.setBounds (row.removeFromRight (70));
        row.removeFromRight (4);
        savePresetButton.setBounds (row.removeFromRight (48));
        row.removeFromRight (6);
        presetNameEditor.setBounds (row);
        c.removeFromTop (rowGap);
        row = c.removeFromTop (rowHeight);
        midiChannelLabel.setBounds (row.removeFromLeft (labelWidth));
        midiChannelCombo.setBounds (row.removeFromLeft (80));
        c.removeFromTop (rowGap);
        row = c.removeFromTop (rowHeight);
        numVoicesLabel.setBounds (row.removeFromLeft (labelWidth));
        numVoicesSlider.setBounds (row.removeFromLeft (100));
        c.removeFromTop (rowGap);
        row = c.removeFromTop (rowHeight);
        mpeToggle.setBounds (row.removeFromLeft (labelWidth));
        mpeBendLabel.setBounds (row.removeFromLeft (80));
        mpeBendRangeSlider.setBounds (row.removeFromLeft (100));
    }

    // Query card content
    {
        auto c = querySoundsCard.getContentBounds();
        auto row = c.removeFromTop (rowHeight);
        queryLabel.setBounds (row.removeFromLeft (labelWidth));
        queryEditor.setBounds (row);
        c.removeFromTop (rowGap);
        row = c.removeFromTop (rowHeight);
        numSoundsLabel.setBounds (row.removeFromLeft (labelWidth));
        numSoundsSlider.setBounds (row.removeFromLeft (100));
        c.removeFromTop (rowGap);
        row = c.removeFromTop (rowHeight);
        lengthLabel.setBounds (row.removeFromLeft (labelWidth));
        minLengthSlider.setBounds (row.removeFromLeft (100));
        lengthToLabel.setBounds (row.removeFromLeft (24));
        maxLengthSlider.setBounds (row.removeFromLeft (100));
        c.removeFromTop (rowGap);
        row = c.removeFromTop (rowHeight);
        layoutLabel.setBounds (row.removeFromLeft (labelWidth));
        noteLayoutCombo.setBounds (row.removeFromLeft (120));
        c.removeFromTop (rowGap + 2);
        replaceAllButton.setBounds (c.removeFromTop (rowHeight).removeFromLeft (210));
        c.removeFromTop (rowGap);
        row = c.removeFromTop (rowHeight);
        replaceOneButton.setBounds (row.removeFromLeft (210));
        row.removeFromLeft (6);
        soundToReplaceSlider.setBounds (row.removeFromLeft (100));
        c.removeFromTop (rowGap);
        row = c.removeFromTop (rowHeight);
        addSoundsButton.setBounds (row.removeFromLeft (210));
        row.removeFromLeft (10);
        statusLabel.setBounds (row);
    }

}
