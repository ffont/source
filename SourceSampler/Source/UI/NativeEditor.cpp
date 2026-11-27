/*
  ==============================================================================

    NativeEditor.cpp

  ==============================================================================
*/

#include "NativeEditor.h"


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
    const int numChannels = juce::jmax (1, (int) levels.size());
    const int meterWidth = juce::jmin (30, (getWidth() - (numChannels - 1) * 6) / numChannels);
    auto b = getLocalBounds();
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto meter = b.removeFromLeft (meterWidth).toFloat();
        b.removeFromLeft (6);
        g.setColour (SourceColours::control);
        g.drawRect (meter, 1.0f);
        const float level = ch < (int) levels.size() ? levels[(size_t) ch] : 0.0f;
        // Map -60..0 dB to 0..1
        const float db = juce::Decibels::gainToDecibels (level, -60.0f);
        const float proportion = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
        g.setColour (db > -1.0f ? SourceColours::warning : SourceColours::accent);
        g.fillRect (meter.reduced (1.0f).removeFromBottom ((meter.getHeight() - 2.0f) * proportion));
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
    const int size = 14;
    const int perRow = juce::jmax (1, getWidth() / (size + 3));
    for (size_t i = 0; i < activeVoices.size(); ++i)
    {
        const int x = (int) (i % (size_t) perRow) * (size + 3);
        const int y = (int) (i / (size_t) perRow) * (size + 3);
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


SourceNativeEditor::SourceNativeEditor (SourceSamplerAudioProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p),
      sourceSampler (p.source),
      actions { [this] (const juce::String& message) { sourceSampler.actionListenerCallback (message); } },
      soundList (actions),
      soundEditor (p.source, actions),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lookAndFeel);

    // Header
    titleLabel.setText ("SOURCE", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (juce::FontOptions (28.0f, juce::Font::bold)));
    addAndMakeVisible (titleLabel);
    statusLabel.setJustificationType (juce::Justification::centredRight);
    statusLabel.setColour (juce::Label::textColourId, SourceColours::dimText);
    addAndMakeVisible (statusLabel);

    // Preset & settings card
    addAndMakeVisible (presetCard);
    setupLabel (presetIndexLabel, "Preset number", presetCard);
    setupNumberSlider (presetIndexSlider, 0, 127, 1, 0, presetCard);
    setupLabel (presetNameLabel, "Preset name", presetCard);
    presetCard.addAndMakeVisible (presetNameEditor);
    presetCard.addAndMakeVisible (savePresetButton);
    presetCard.addAndMakeVisible (loadPresetButton);
    presetCard.addAndMakeVisible (previousPresetButton);
    presetCard.addAndMakeVisible (nextPresetButton);
    savePresetButton.onClick = [this]
    {
        sendAction (ACTION_SAVE_PRESET, { SourceActionSender::sanitize (presetNameEditor.getText()), juce::String ((int) presetIndexSlider.getValue()) });
    };
    loadPresetButton.onClick = [this] { sendAction (ACTION_LOAD_PRESET, { juce::String ((int) presetIndexSlider.getValue()) }); };
    previousPresetButton.onClick = [this]
    {
        const int index = juce::jmax (0, (int) sourceSampler.state.getProperty (SourceIDs::currentPresetIndex, 0) - 1);
        sendAction (ACTION_LOAD_PRESET, { juce::String (index) });
    };
    nextPresetButton.onClick = [this]
    {
        const int index = juce::jmin (127, (int) sourceSampler.state.getProperty (SourceIDs::currentPresetIndex, -1) + 1);
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
    presetCard.addAndMakeVisible (voicesComponent);

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
    addAndMakeVisible (querySoundsCard);
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
    setupNumberSlider (soundToReplaceSlider, 1, 128, 1, 1, querySoundsCard);

    // Reverb card
    addAndMakeVisible (reverbCard);
    const std::array<std::pair<juce::Identifier, const char*>, 6> reverbParams {{
        { SourceIDs::reverbRoomSize, "Room size" }, { SourceIDs::reverbDamping, "Damping" }, { SourceIDs::reverbWidth, "Width" },
        { SourceIDs::reverbFreezeMode, "Freeze" }, { SourceIDs::reverbWetLevel, "Wet level" }, { SourceIDs::reverbDryLevel, "Dry level" } }};
    for (size_t i = 0; i < reverbControls.size(); ++i)
    {
        auto& control = reverbControls[i];
        control.id = reverbParams[i].first;
        setupLabel (control.label, reverbParams[i].second, reverbCard);
        control.slider.setSliderStyle (juce::Slider::LinearHorizontal);
        control.slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 40, 18);
        control.slider.setRange (0.0, 1.0, 0.01);
        control.slider.onValueChange = [this] { sendReverbParameters(); };
        reverbCard.addAndMakeVisible (control.slider);
    }

    // Meters card
    addAndMakeVisible (metersCard);
    metersCard.addAndMakeVisible (levelMeters);

    // Sounds
    soundList.onSelectionChanged = [this] (const juce::String& uuid) { soundEditor.setSound (uuid); };
    addAndMakeVisible (soundList);
    addAndMakeVisible (soundEditor);

    // Keyboard
    keyboard.setAvailableRange (21, 108);
    keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xffe8e8e8));
    keyboard.setColour (juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour (0xff202020));
    keyboard.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour (0xff606060));
    addAndMakeVisible (keyboard);

    sourceSampler.state.addListener (this);
    updateFromState();

    setResizable (true, true);
    setResizeLimits (960, 680, 2400, 1600);
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
    const auto snapshotPath = juce::SystemStats::getEnvironmentVariable ("SOURCE_DEBUG_SNAPSHOT", {});
    if (snapshotPath.isNotEmpty())
        juce::Timer::callAfterDelay (3500, [safe = juce::Component::SafePointer<SourceNativeEditor> (this), snapshotPath]
        {
            if (safe == nullptr)
                return;
            if (safe->soundList.getNumSounds() > 0)
                safe->soundList.selectSound (safe->soundList.getItems().front().uuid);
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

void SourceNativeEditor::sendAction (const juce::String& actionName, const juce::StringArray& parameters)
{
    actions.send (actionName, parameters);
}

void SourceNativeEditor::sendReverbParameters()
{
    // Order defined by ACTION_SET_REVERB_PARAMETERS: roomSize, damping, wetLevel, dryLevel, width, freezeMode
    auto valueOf = [this] (const juce::Identifier& id)
    {
        for (auto& control : reverbControls)
            if (control.id == id)
                return juce::String (control.slider.getValue());
        return juce::String();
    };
    sendAction (ACTION_SET_REVERB_PARAMETERS, { valueOf (SourceIDs::reverbRoomSize), valueOf (SourceIDs::reverbDamping),
                                                valueOf (SourceIDs::reverbWetLevel), valueOf (SourceIDs::reverbDryLevel),
                                                valueOf (SourceIDs::reverbWidth), valueOf (SourceIDs::reverbFreezeMode) });
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

    if (! presetIndexSlider.isMouseButtonDown())
        presetIndexSlider.setValue (juce::jmax (0, (int) state.getProperty (SourceIDs::currentPresetIndex, 0)), juce::dontSendNotification);
    if (! presetNameEditor.hasKeyboardFocus (true))
        presetNameEditor.setText (preset.getProperty (SourceIDs::name).toString(), false);
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
    for (auto& control : reverbControls)
        if (! control.slider.isMouseButtonDown())
            control.slider.setValue ((float) preset.getProperty (control.id, 0.0f), juce::dontSendNotification);

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

    juce::String status;
    if (sourceSampler.isQueryingSounds())
    {
        status = "Searching Freesound...";
    }
    else
    {
        const int numResults = sourceSampler.state.getProperty (SourceIDs::numResultsLastQuery, -2);
        if (numResults >= 0)
            status << numResults << " results in last query  |  ";
        status << soundList.getNumSounds() << " sounds loaded";
        const int lastNote = sourceSampler.getLastReceivedMidiNote();
        if (lastNote >= 0)
            status << "  |  last note: " << juce::MidiMessage::getMidiNoteName (lastNote, true, true, 3);
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
    auto b = getLocalBounds().reduced (8);

    auto header = b.removeFromTop (36);
    titleLabel.setBounds (header.removeFromLeft (200));
    statusLabel.setBounds (header);
    b.removeFromTop (4);

    keyboard.setBounds (b.removeFromBottom (64));
    {
        // Make the visible keys fill the whole width (from C1 up to as many keys as fit)
        const int numWhiteKeys = 7 * 7 + 1;  // 7 octaves
        keyboard.setKeyWidth (juce::jmax (8.0f, (float) keyboard.getWidth() / (float) numWhiteKeys));
        keyboard.setLowestVisibleKey (24);
    }
    b.removeFromBottom (8);

    // Top row of cards
    auto top = b.removeFromTop (236);
    const int gap = 8;
    const int metersWidth = 100;
    const int cardWidth = (top.getWidth() - metersWidth - 3 * gap) / 3;
    presetCard.setBounds (top.removeFromLeft (cardWidth));
    top.removeFromLeft (gap);
    querySoundsCard.setBounds (top.removeFromLeft (cardWidth));
    top.removeFromLeft (gap);
    reverbCard.setBounds (top.removeFromLeft (cardWidth));
    top.removeFromLeft (gap);
    metersCard.setBounds (top);
    b.removeFromTop (gap);

    const int rowHeight = 22;
    const int rowGap = 5;
    const int labelWidth = 100;

    // Preset card content
    {
        auto c = presetCard.getContentBounds();
        auto row = c.removeFromTop (rowHeight);
        presetIndexLabel.setBounds (row.removeFromLeft (labelWidth));
        presetIndexSlider.setBounds (row.removeFromLeft (100));
        row.removeFromLeft (6);
        savePresetButton.setBounds (row.removeFromLeft (50));
        row.removeFromLeft (4);
        loadPresetButton.setBounds (row.removeFromLeft (50));
        c.removeFromTop (rowGap);
        row = c.removeFromTop (rowHeight);
        presetNameLabel.setBounds (row.removeFromLeft (labelWidth));
        nextPresetButton.setBounds (row.removeFromRight (24));
        row.removeFromRight (2);
        previousPresetButton.setBounds (row.removeFromRight (24));
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
        c.removeFromTop (rowGap + 2);
        voicesComponent.setBounds (c);
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
        addSoundsButton.setBounds (c.removeFromTop (rowHeight).removeFromLeft (210));
    }

    // Reverb card content
    {
        auto c = reverbCard.getContentBounds();
        for (auto& control : reverbControls)
        {
            auto row = c.removeFromTop (rowHeight);
            control.label.setBounds (row.removeFromLeft (80));
            control.slider.setBounds (row);
            c.removeFromTop (rowGap + 2);
        }
    }

    levelMeters.setBounds (metersCard.getContentBounds());

    // Sounds
    auto listArea = b.removeFromLeft (juce::jmax (300, b.getWidth() / 3));
    soundList.setBounds (listArea);
    b.removeFromLeft (gap);
    soundEditor.setBounds (b);
}
