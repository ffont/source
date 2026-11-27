/*
  ==============================================================================

    SoundEditorComponent.cpp

  ==============================================================================
*/

#include "SoundEditorComponent.h"
#include "SourceSampler.h"


//==============================================================================
// WaveformComponent

void WaveformComponent::setAudio (const juce::AudioBuffer<float>* audio)
{
    peaks.clear();
    if (audio != nullptr && audio->getNumSamples() > 0)
    {
        const int numSamples = audio->getNumSamples();
        const int numBins = juce::jmin (numPeakBins, numSamples);
        peaks.resize ((size_t) numBins, { 0.0f, 0.0f });
        for (int ch = 0; ch < audio->getNumChannels(); ++ch)
        {
            const float* data = audio->getReadPointer (ch);
            for (int bin = 0; bin < numBins; ++bin)
            {
                const int start = (int) ((juce::int64) bin * numSamples / numBins);
                const int end = juce::jmax (start + 1, (int) ((juce::int64) (bin + 1) * numSamples / numBins));
                auto range = juce::FloatVectorOperations::findMinAndMax (data + start, end - start);
                auto& peak = peaks[(size_t) bin];
                peak.first = juce::jmin (peak.first, range.getStart());
                peak.second = juce::jmax (peak.second, range.getEnd());
            }
        }
    }
    repaint();
}

void WaveformComponent::setMarkers (float start, float end, float loopStart, float loopEnd, bool showLoop)
{
    if (start == startPos && end == endPos && loopStart == loopStartPos && loopEnd == loopEndPos && showLoop == loopVisible)
        return;
    startPos = start; endPos = end; loopStartPos = loopStart; loopEndPos = loopEnd; loopVisible = showLoop;
    repaint();
}

void WaveformComponent::setPlayheads (std::vector<float> positions)
{
    if (positions == playheads)
        return;
    playheads = std::move (positions);
    repaint();
}

void WaveformComponent::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (SourceColours::background);
    g.fillRoundedRectangle (bounds, 4.0f);

    if (peaks.empty())
    {
        g.setColour (SourceColours::dimText);
        g.setFont (juce::Font (juce::FontOptions (13.0f)));
        g.drawText (messageText, getLocalBounds(), juce::Justification::centred);
        return;
    }

    const float w = bounds.getWidth();
    const float h = bounds.getHeight();
    const float midY = bounds.getCentreY();
    auto xFor = [w] (float relative) { return juce::jlimit (0.0f, 1.0f, relative) * w; };

    // Selected region (start-end)
    g.setColour (SourceColours::control.withAlpha (0.35f));
    g.fillRect (juce::Rectangle<float>::leftTopRightBottom (xFor (startPos), 0.0f, xFor (endPos), h));

    // Waveform
    juce::Path path;
    const int numColumns = juce::jmax (1, (int) w);
    for (int x = 0; x < numColumns; ++x)
    {
        const size_t firstBin = (size_t) ((juce::int64) x * (juce::int64) peaks.size() / numColumns);
        const size_t lastBin = juce::jmax (firstBin + 1, (size_t) ((juce::int64) (x + 1) * (juce::int64) peaks.size() / numColumns));
        float minV = 0.0f, maxV = 0.0f;
        for (size_t b = firstBin; b < juce::jmin (lastBin, peaks.size()); ++b)
        {
            minV = juce::jmin (minV, peaks[b].first);
            maxV = juce::jmax (maxV, peaks[b].second);
        }
        const float top = midY - juce::jlimit (0.0f, 1.0f, maxV) * h * 0.48f;
        const float bottom = midY - juce::jlimit (-1.0f, 0.0f, minV) * h * 0.48f;
        path.addRectangle ((float) x, top, 1.0f, juce::jmax (1.0f, bottom - top));
    }
    g.setColour (SourceColours::accent.withAlpha (0.85f));
    g.fillPath (path);

    // Markers
    g.setColour (SourceColours::text);
    g.drawVerticalLine ((int) xFor (startPos), 0.0f, h);
    g.drawVerticalLine (juce::jmin ((int) w - 1, (int) xFor (endPos)), 0.0f, h);
    if (loopVisible)
    {
        g.setColour (SourceColours::warning);
        g.drawVerticalLine ((int) xFor (loopStartPos), 0.0f, h);
        g.drawVerticalLine (juce::jmin ((int) w - 1, (int) xFor (loopEndPos)), 0.0f, h);
    }

    // Playheads
    g.setColour (juce::Colours::white);
    for (auto position : playheads)
        if (position >= 0.0f)
            g.fillRect (xFor (position) - 1.0f, 0.0f, 2.0f, h);
}


//==============================================================================
// ParameterControl

static juce::StringArray getChoicesForParameter (const SoundParameterInfo& info)
{
    const juce::String name (info.name);
    if (name == "launchMode")      return { "Gate", "Loop", "Loop ping-pong", "Trigger", "Freeze" };
    if (name == "noteMappingMode") return { "Pitch", "Slice", "Pitch & slice", "Repeat" };
    if (name == "midiChannel")
    {
        juce::StringArray channels { "Global" };
        for (int i = 1; i <= 16; ++i)
            channels.add ("Ch " + juce::String (i));
        return channels;
    }
    return {};
}

static juce::String getDisplayNameForParameter (const juce::String& name)
{
    static const std::map<juce::String, juce::String> names {
        { "launchMode", "Launch mode" }, { "startPosition", "Start" }, { "endPosition", "End" },
        { "loopStartPosition", "Loop start" }, { "loopEndPosition", "Loop end" }, { "loopXFadeNSamples", "Loop x-fade" },
        { "reverse", "Reverse" }, { "noteMappingMode", "Note mapping" }, { "numSlices", "Num slices" },
        { "playheadPosition", "Playhead" }, { "freezePlayheadSpeed", "Freeze speed" }, { "filterCutoff", "Cutoff" },
        { "filterRessonance", "Resonance" }, { "filterKeyboardTracking", "KB tracking" }, { "filterAttack", "Attack" },
        { "filterDecay", "Decay" }, { "filterSustain", "Sustain" }, { "filterRelease", "Release" },
        { "filterADSR2CutoffAmt", "Env amount" }, { "gain", "Gain (dB)" }, { "attack", "Attack" }, { "decay", "Decay" },
        { "sustain", "Sustain" }, { "release", "Release" }, { "pan", "Pan" }, { "pitch", "Pitch (st)" },
        { "pitchBendRangeUp", "Bend up" }, { "pitchBendRangeDown", "Bend down" }, { "mod2CutoffAmt", "Mod > cutoff" },
        { "mod2GainAmt", "Mod > gain" }, { "mod2PitchAmt", "Mod > pitch" }, { "mod2PlayheadPos", "Mod > playhead" },
        { "vel2CutoffAmt", "Vel > cutoff" }, { "vel2GainAmt", "Vel > gain" }, { "velSensitivity", "Vel sensitivity" },
        { "midiChannel", "MIDI channel" }, { "pitchShift", "Pitch shift" }, { "timeStretch", "Time stretch" },
    };
    auto it = names.find (name);
    return it != names.end() ? it->second : name;
}

ParameterControl::ParameterControl (const SoundParameterInfo& infoToUse, std::function<void (const SoundParameterInfo&, float)> changeCallback)
    : info (infoToUse), onChange (std::move (changeCallback))
{
    label.setText (getDisplayNameForParameter (info.name), juce::dontSendNotification);
    label.setFont (juce::Font (juce::FontOptions (12.0f)));
    label.setColour (juce::Label::textColourId, SourceColours::dimText);
    label.setTooltip (info.description);
    addAndMakeVisible (label);

    const auto choices = getChoicesForParameter (info);
    if (! choices.isEmpty())
    {
        comboBox = std::make_unique<juce::ComboBox>();
        comboBox->addItemList (choices, 1);
        comboBox->onChange = [this] { onChange (info, (float) (comboBox->getSelectedItemIndex() + (int) info.minValue)); };
        addAndMakeVisible (*comboBox);
    }
    else if (info.isInt && info.minValue == 0.0f && info.maxValue == 1.0f)
    {
        toggle = std::make_unique<juce::ToggleButton>();
        toggle->onClick = [this] { onChange (info, toggle->getToggleState() ? 1.0f : 0.0f); };
        addAndMakeVisible (*toggle);
    }
    else
    {
        slider = std::make_unique<juce::Slider> (juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
        slider->setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 18);
        juce::NormalisableRange<double> range (info.minValue, info.maxValue, info.isInt ? 1.0 : 0.0);
        const juce::String name (info.name);
        if (name == "filterCutoff" || name == "freezePlayheadSpeed" || name == "loopXFadeNSamples")
            range.setSkewForCentre (name == "filterCutoff" ? 1000.0 : (name == "loopXFadeNSamples" ? 2000.0 : 300.0));
        else if (name == "attack" || name == "decay" || name == "release" || name == "filterAttack" || name == "filterDecay" || name == "filterRelease")
            range.setSkewForCentre (0.15);
        slider->setNormalisableRange (range);
        slider->setNumDecimalPlacesToDisplay (info.isInt ? 0 : (info.maxValue - info.minValue > 50.0f ? 1 : 2));
        slider->setDoubleClickReturnValue (true, info.defaultValue);
        slider->onValueChange = [this] { onChange (info, (float) slider->getValue()); };
        addAndMakeVisible (*slider);
    }
    setValue (info.defaultValue);
}

void ParameterControl::setValue (float value)
{
    if (slider != nullptr)
        slider->setValue (value, juce::dontSendNotification);
    else if (comboBox != nullptr)
        comboBox->setSelectedItemIndex (juce::roundToInt (value - info.minValue), juce::dontSendNotification);
    else if (toggle != nullptr)
        toggle->setToggleState (value > 0.5f, juce::dontSendNotification);
}

bool ParameterControl::isBeingEdited() const
{
    return (slider != nullptr && slider->isMouseButtonDown()) || (comboBox != nullptr && comboBox->isPopupActive());
}

void ParameterControl::resized()
{
    auto b = getLocalBounds();
    label.setBounds (b.removeFromLeft (96));
    if (slider != nullptr)        slider->setBounds (b);
    else if (comboBox != nullptr) comboBox->setBounds (b.withSizeKeepingCentre (b.getWidth(), 20));
    else if (toggle != nullptr)   toggle->setBounds (b.removeFromLeft (30));
}


//==============================================================================
// MidiMappingsComponent

MidiMappingsComponent::MappingRow::MappingRow (MidiMappingsComponent& owner)
{
    ccNumber.setSliderStyle (juce::Slider::IncDecButtons);
    ccNumber.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 36, 20);
    ccNumber.setRange (0, 127, 1);
    ccNumber.setTooltip ("MIDI CC number");
    addAndMakeVisible (ccNumber);

    int itemId = 1;
    for (const auto& info : getSoundParametersInfo())
        if (info.midiCCEnabled)
            parameter.addItem (info.name, itemId++);
    parameter.setSelectedItemIndex (0, juce::dontSendNotification);
    addAndMakeVisible (parameter);

    for (auto* s : { &minRange, &maxRange })
    {
        s->setSliderStyle (juce::Slider::LinearHorizontal);
        s->setTextBoxStyle (juce::Slider::TextBoxRight, false, 34, 18);
        s->setRange (0.0, 1.0, 0.01);
        addAndMakeVisible (s);
    }
    minRange.setTooltip ("Minimum (normalised) parameter value");
    maxRange.setTooltip ("Maximum (normalised) parameter value");
    maxRange.setValue (1.0, juce::dontSendNotification);

    saveButton.onClick = [this, &owner] { owner.sendMapping (*this); };
    removeButton.onClick = [this, &owner] { owner.actions.send (ACTION_REMOVE_CC_MAPPING, { owner.currentSoundUUID, uuid }); };
    addAndMakeVisible (saveButton);
    addAndMakeVisible (removeButton);
}

void MidiMappingsComponent::MappingRow::resized()
{
    auto b = getLocalBounds();
    removeButton.setBounds (b.removeFromRight (22).withSizeKeepingCentre (22, 20));
    b.removeFromRight (4);
    saveButton.setBounds (b.removeFromRight (44).withSizeKeepingCentre (44, 20));
    b.removeFromRight (4);
    ccNumber.setBounds (b.removeFromLeft (84));
    b.removeFromLeft (4);
    parameter.setBounds (b.removeFromLeft (juce::jmax (100, b.getWidth() / 3)).withSizeKeepingCentre (juce::jmax (100, b.getWidth() / 3), 20));
    b.removeFromLeft (4);
    minRange.setBounds (b.removeFromLeft (b.getWidth() / 2));
    maxRange.setBounds (b);
}

MidiMappingsComponent::MidiMappingsComponent (SourceActionSender actionSender) : actions (std::move (actionSender))
{
    addButton.onClick = [this]
    {
        auto* row = rows.add (new MappingRow (*this));
        row->uuid = juce::Uuid().toString();
        row->ccNumber.setValue (1, juce::dontSendNotification);
        addAndMakeVisible (row);
        sendMapping (*row);
    };
    addAndMakeVisible (addButton);
}

void MidiMappingsComponent::sendMapping (MappingRow& row)
{
    if (currentSoundUUID.isEmpty())
        return;
    actions.send (ACTION_ADD_OR_UPDATE_CC_MAPPING, {
        currentSoundUUID, row.uuid, juce::String ((int) row.ccNumber.getValue()), row.parameter.getText(),
        juce::String (row.minRange.getValue()), juce::String (row.maxRange.getValue()) });
}

void MidiMappingsComponent::updateFromState (const juce::String& soundUUID, const juce::ValueTree& soundState)
{
    currentSoundUUID = soundUUID;
    addButton.setEnabled (soundUUID.isNotEmpty());

    std::vector<juce::ValueTree> mappings;
    for (const auto& child : soundState)
        if (child.hasType (SourceIDs::MIDI_CC_MAPPING))
            mappings.push_back (child);

    const bool needsRebuild = (int) mappings.size() != rows.size();
    if (needsRebuild)
    {
        rows.clear();
        for (size_t i = 0; i < mappings.size(); ++i)
            addAndMakeVisible (rows.add (new MappingRow (*this)));
    }
    for (size_t i = 0; i < mappings.size(); ++i)
    {
        auto* row = rows[(int) i];
        const auto& mapping = mappings[i];
        row->uuid = mapping.getProperty (SourceIDs::uuid).toString();
        if (! row->ccNumber.isMouseButtonDown())
            row->ccNumber.setValue ((int) mapping.getProperty (SourceIDs::ccNumber, 1), juce::dontSendNotification);
        const juce::String paramName = mapping.getProperty (SourceIDs::parameterName).toString();
        for (int item = 0; item < row->parameter.getNumItems(); ++item)
            if (row->parameter.getItemText (item) == paramName)
                row->parameter.setSelectedItemIndex (item, juce::dontSendNotification);
        if (! row->minRange.isMouseButtonDown())
            row->minRange.setValue ((float) mapping.getProperty (SourceIDs::minRange, 0.0f), juce::dontSendNotification);
        if (! row->maxRange.isMouseButtonDown())
            row->maxRange.setValue ((float) mapping.getProperty (SourceIDs::maxRange, 1.0f), juce::dontSendNotification);
    }
    if (needsRebuild)
        resized();
}

int MidiMappingsComponent::getIdealHeight() const
{
    return 28 + 26 * rows.size() + 30;
}

void MidiMappingsComponent::paint (juce::Graphics& g)
{
    g.setColour (SourceColours::text);
    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    g.drawText ("MIDI CC mappings", getLocalBounds().removeFromTop (24), juce::Justification::centredLeft);
}

void MidiMappingsComponent::resized()
{
    auto b = getLocalBounds();
    b.removeFromTop (28);
    for (auto* row : rows)
        row->setBounds (b.removeFromTop (26).reduced (0, 2));
    b.removeFromTop (4);
    addButton.setBounds (b.removeFromTop (22).removeFromLeft (120));
}


//==============================================================================
// MpeMappingsComponent

static juce::String getMpeSourceDisplayName (const juce::String& name)
{
    if (name == MPE_SOURCE_SLIDE)  return "Slide";
    if (name == MPE_SOURCE_GLIDE)  return "Glide";
    if (name == MPE_SOURCE_STRIKE) return "Strike";
    return name;
}

MpeMappingsComponent::MappingRow::MappingRow (MpeMappingsComponent& owner)
{
    const auto sourceNames = SourceHelpers::getMpeSourceNames();
    for (int i = 0; i < sourceNames.size(); ++i)
        source.addItem (getMpeSourceDisplayName (sourceNames[i]), i + 1);
    source.setSelectedItemIndex (0, juce::dontSendNotification);
    source.setTooltip ("Slide: CC74, Glide: per-note pitch bend, Strike: note velocity. Per-note pressure is aftertouch: use the Modulation section");
    addAndMakeVisible (source);

    int itemId = 1;
    for (const auto& info : getSoundParametersInfo())
        if (info.midiCCEnabled && ! info.isInt)
            parameter.addItem (info.name, itemId++);
    parameter.setSelectedItemIndex (0, juce::dontSendNotification);
    addAndMakeVisible (parameter);

    for (auto* s : { &minRange, &maxRange, &curve })
    {
        s->setSliderStyle (juce::Slider::LinearHorizontal);
        s->setTextBoxStyle (juce::Slider::TextBoxRight, false, 34, 18);
        s->setRange (0.0, 1.0, 0.01);
        s->onDragEnd = [this, &owner] { owner.sendMapping (*this); };
        addAndMakeVisible (s);
    }
    curve.setRange (-1.0, 1.0, 0.01);
    curve.setDoubleClickReturnValue (true, 0.0);
    minRange.setTooltip ("Parameter value (normalised) when the MPE dimension is at its minimum");
    maxRange.setTooltip ("Parameter value (normalised) when the MPE dimension is at its maximum");
    curve.setTooltip ("Response curve: 0 = linear, positive = slow start, negative = fast start");
    maxRange.setValue (1.0, juce::dontSendNotification);

    source.onChange = [this, &owner] { owner.sendMapping (*this); };
    parameter.onChange = [this, &owner] { owner.sendMapping (*this); };
    removeButton.onClick = [this, &owner] { owner.actions.send (ACTION_REMOVE_MPE_MAPPING, { owner.currentSoundUUID, uuid }); };
    addAndMakeVisible (removeButton);
}

bool MpeMappingsComponent::MappingRow::isBeingEdited() const
{
    return minRange.isMouseButtonDown() || maxRange.isMouseButtonDown() || curve.isMouseButtonDown()
        || source.isPopupActive() || parameter.isPopupActive();
}

void MpeMappingsComponent::MappingRow::resized()
{
    auto b = getLocalBounds();
    removeButton.setBounds (b.removeFromRight (22).withSizeKeepingCentre (22, 20));
    b.removeFromRight (4);
    source.setBounds (b.removeFromLeft (76).withSizeKeepingCentre (76, 20));
    b.removeFromLeft (4);
    parameter.setBounds (b.removeFromLeft (juce::jmax (110, b.getWidth() / 4)).withSizeKeepingCentre (juce::jmax (110, b.getWidth() / 4), 20));
    b.removeFromLeft (4);
    const int sliderWidth = b.getWidth() / 3;
    minRange.setBounds (b.removeFromLeft (sliderWidth));
    maxRange.setBounds (b.removeFromLeft (sliderWidth));
    curve.setBounds (b);
}

MpeMappingsComponent::MpeMappingsComponent (SourceActionSender actionSender) : actions (std::move (actionSender))
{
    addButton.onClick = [this]
    {
        auto* row = rows.add (new MappingRow (*this));
        row->uuid = juce::Uuid().toString();
        addAndMakeVisible (row);
        sendMapping (*row);
    };
    addAndMakeVisible (addButton);
    columnsLabel.setFont (juce::Font (juce::FontOptions (11.0f)));
    columnsLabel.setColour (juce::Label::textColourId, SourceColours::dimText);
    addAndMakeVisible (columnsLabel);
}

void MpeMappingsComponent::sendMapping (MappingRow& row)
{
    if (currentSoundUUID.isEmpty())
        return;
    actions.send (ACTION_ADD_OR_UPDATE_MPE_MAPPING, {
        currentSoundUUID, row.uuid, SourceHelpers::getMpeSourceNames()[row.source.getSelectedItemIndex()], row.parameter.getText(),
        juce::String (row.minRange.getValue()), juce::String (row.maxRange.getValue()), juce::String (row.curve.getValue()) });
}

void MpeMappingsComponent::updateFromState (const juce::String& soundUUID, const juce::ValueTree& soundState, bool mpeEnabled)
{
    currentSoundUUID = soundUUID;
    if (isMpeEnabled != mpeEnabled)
    {
        isMpeEnabled = mpeEnabled;
        repaint();
    }
    addButton.setEnabled (soundUUID.isNotEmpty());

    std::vector<juce::ValueTree> mappings;
    for (const auto& child : soundState)
        if (child.hasType (SourceIDs::MPE_MAPPING))
            mappings.push_back (child);

    const bool needsRebuild = (int) mappings.size() != rows.size();
    if (needsRebuild)
    {
        rows.clear();
        for (size_t i = 0; i < mappings.size(); ++i)
            addAndMakeVisible (rows.add (new MappingRow (*this)));
    }
    const auto sourceNames = SourceHelpers::getMpeSourceNames();
    for (size_t i = 0; i < mappings.size(); ++i)
    {
        auto* row = rows[(int) i];
        const auto& mapping = mappings[i];
        row->uuid = mapping.getProperty (SourceIDs::uuid).toString();
        if (row->isBeingEdited())
            continue;
        row->source.setSelectedItemIndex (juce::jmax (0, sourceNames.indexOf (mapping.getProperty (SourceIDs::mpeSource).toString())), juce::dontSendNotification);
        const juce::String paramName = mapping.getProperty (SourceIDs::parameterName).toString();
        for (int item = 0; item < row->parameter.getNumItems(); ++item)
            if (row->parameter.getItemText (item) == paramName)
                row->parameter.setSelectedItemIndex (item, juce::dontSendNotification);
        row->minRange.setValue ((float) mapping.getProperty (SourceIDs::minRange, 0.0f), juce::dontSendNotification);
        row->maxRange.setValue ((float) mapping.getProperty (SourceIDs::maxRange, 1.0f), juce::dontSendNotification);
        row->curve.setValue ((float) mapping.getProperty (SourceIDs::curve, 0.0f), juce::dontSendNotification);
    }
    columnsLabel.setText (rows.isEmpty() ? juce::String() : "Source / parameter / min / max / curve", juce::dontSendNotification);
    if (needsRebuild)
        resized();
}

int MpeMappingsComponent::getIdealHeight() const
{
    return 28 + 16 + 26 * rows.size() + 30;
}

void MpeMappingsComponent::paint (juce::Graphics& g)
{
    auto header = getLocalBounds().removeFromTop (24);
    g.setColour (SourceColours::text);
    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    g.drawText ("MPE mappings", header, juce::Justification::centredLeft);
    if (! isMpeEnabled)
    {
        g.setColour (SourceColours::warning);
        g.setFont (juce::Font (juce::FontOptions (11.0f)));
        g.drawText ("MPE input is disabled (enable it in Preset & settings)", header.withTrimmedLeft (110), juce::Justification::centredLeft);
    }
}

void MpeMappingsComponent::resized()
{
    auto b = getLocalBounds();
    b.removeFromTop (28);
    columnsLabel.setBounds (b.removeFromTop (16));
    for (auto* row : rows)
        row->setBounds (b.removeFromTop (26).reduced (0, 2));
    b.removeFromTop (4);
    addButton.setBounds (b.removeFromTop (22).removeFromLeft (130));
}


//==============================================================================
// SoundEditorComponent

/** Short name for a Creative Commons license URL, e.g. "http://creativecommons.org/licenses/by-nc/3.0/" -> "CC BY-NC 3.0". */
static juce::String getShortLicenseName (const juce::String& licenseURL)
{
    if (licenseURL.contains ("publicdomain/zero"))
        return "CC0";
    if (licenseURL.contains ("sampling+"))
        return "CC Sampling+";
    if (licenseURL.contains ("/licenses/"))
    {
        auto parts = juce::StringArray::fromTokens (licenseURL.fromFirstOccurrenceOf ("/licenses/", false, false), "/", {});
        parts.removeEmptyStrings();
        if (parts.size() >= 1)
            return "CC " + parts[0].toUpperCase() + (parts.size() >= 2 ? " " + parts[1] : juce::String());
    }
    return licenseURL;
}

namespace
{
    const std::vector<std::pair<juce::String, juce::StringArray>>& getParameterGroups()
    {
        static const std::vector<std::pair<juce::String, juce::StringArray>> groups {
            { "Playback",   { "launchMode", "startPosition", "endPosition", "loopStartPosition", "loopEndPosition", "loopXFadeNSamples", "reverse", "playheadPosition", "freezePlayheadSpeed" } },
            { "Mapping",    { "noteMappingMode", "numSlices", "midiChannel" } },
            { "Pitch",      { "pitch", "pitchBendRangeUp", "pitchBendRangeDown", "pitchShift", "timeStretch" } },
            { "Amplitude",  { "gain", "pan", "attack", "decay", "sustain", "release", "vel2GainAmt", "velSensitivity" } },
            { "Filter",     { "filterCutoff", "filterRessonance", "filterKeyboardTracking", "filterAttack", "filterDecay", "filterSustain", "filterRelease", "filterADSR2CutoffAmt", "vel2CutoffAmt" } },
            { "Modulation", { "mod2CutoffAmt", "mod2GainAmt", "mod2PitchAmt", "mod2PlayheadPos" } },
        };
        return groups;
    }
}

SoundEditorComponent::SoundEditorComponent (SourceSampler& s, SourceActionSender actionSender)
    : sampler (s), actions (actionSender), midiMappings (actionSender), mpeMappings (actionSender)
{
    titleLabel.setFont (juce::Font (juce::FontOptions (18.0f, juce::Font::bold)));
    addAndMakeVisible (titleLabel);
    detailsLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    detailsLabel.setColour (juce::Label::textColourId, SourceColours::dimText);
    addAndMakeVisible (detailsLabel);

    waveform.onClick = [this] { if (soundUUID.isNotEmpty()) actions.send (ACTION_PLAY_SOUND, { soundUUID }); };
    waveform.setTooltip ("Click to play the sound");
    addAndMakeVisible (waveform);

    rootNoteLabel.setText ("Root note", juce::dontSendNotification);
    rootNoteLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    rootNoteLabel.setColour (juce::Label::textColourId, SourceColours::dimText);
    addAndMakeVisible (rootNoteLabel);
    rootNoteSlider.setSliderStyle (juce::Slider::IncDecButtons);
    rootNoteSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 60, 20);
    rootNoteSlider.setRange (0, 127, 1);
    rootNoteSlider.textFromValueFunction = [] (double v) { return juce::MidiMessage::getMidiNoteName ((int) v, true, true, 3); };
    rootNoteSlider.valueFromTextFunction = [] (const juce::String& text)
    {
        for (int n = 0; n < 128; ++n)
            if (juce::MidiMessage::getMidiNoteName (n, true, true, 3).equalsIgnoreCase (text.trim()))
                return (double) n;
        return text.getDoubleValue();
    };
    rootNoteSlider.onValueChange = [this]
    {
        if (soundUUID.isNotEmpty())
            actions.send (ACTION_SET_SOUND_ASSIGNED_NOTES, { soundUUID, "", juce::String ((int) rootNoteSlider.getValue()) });
    };
    addAndMakeVisible (rootNoteSlider);

    // Create one control per parameter, organised in groups (parameters not in any group go to "Other")
    juce::StringArray grouped;
    for (const auto& [groupName, names] : getParameterGroups())
    {
        Group group;
        group.name = groupName;
        for (const auto& name : names)
        {
            if (auto* info = findSoundParameterInfo (name))
            {
                group.controls.push_back (parameterControls.add (new ParameterControl (*info, [this] (const SoundParameterInfo& i, float v) { parameterChanged (i, v); })));
                grouped.add (name);
            }
        }
        groups.push_back (std::move (group));
    }
    Group other;
    other.name = "Other";
    for (const auto& info : getSoundParametersInfo())
        if (! grouped.contains (info.name))
            other.controls.push_back (parameterControls.add (new ParameterControl (info, [this] (const SoundParameterInfo& i, float v) { parameterChanged (i, v); })));
    if (! other.controls.empty())
        groups.push_back (std::move (other));

    for (auto& group : groups)
    {
        group.header = std::make_unique<juce::Label> (group.name, group.name);
        group.header->setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
        group.header->setColour (juce::Label::textColourId, SourceColours::accent);
        parametersContent.addAndMakeVisible (*group.header);
        for (auto* control : group.controls)
            parametersContent.addAndMakeVisible (control);
    }
    parametersContent.addAndMakeVisible (midiMappings);
    parametersContent.addAndMakeVisible (mpeMappings);

    parametersViewport.setViewedComponent (&parametersContent, false);
    parametersViewport.setScrollBarsShown (true, false);
    addAndMakeVisible (parametersViewport);

    setSound ({});
}

juce::ValueTree SoundEditorComponent::getSoundState() const
{
    if (soundUUID.isEmpty())
        return {};
    return sampler.state.getChildWithName (SourceIDs::PRESET).getChildWithProperty (SourceIDs::uuid, soundUUID);
}

void SoundEditorComponent::setSound (const juce::String& newSoundUUID)
{
    soundUUID = newSoundUUID;
    loadedWaveformForSampleUUID = {};
    waveform.setAudio (nullptr);
    waveform.setPlayheads ({});
    updateFromState();
    resized();
}

void SoundEditorComponent::parameterChanged (const SoundParameterInfo& info, float value)
{
    // An empty sound UUID means "all sounds"
    if (info.isInt)
        actions.send (ACTION_SET_SOUND_PARAMETER_INT, { soundUUID, info.name, juce::String (juce::roundToInt (value)) });
    else
        actions.send (ACTION_SET_SOUND_PARAMETER_FLOAT, { soundUUID, info.name, juce::String (value) });
}

void SoundEditorComponent::updateFromState()
{
    const bool allSounds = soundUUID.isEmpty();
    auto soundState = getSoundState();
    if (! allSounds && ! soundState.isValid())
        return;  // Sound was removed, the owner will select another one

    waveform.setVisible (! allSounds);
    rootNoteSlider.setVisible (! allSounds);
    rootNoteLabel.setVisible (! allSounds);
    midiMappings.setVisible (! allSounds);
    mpeMappings.setVisible (! allSounds);

    if (allSounds)
    {
        titleLabel.setText ("All sounds", juce::dontSendNotification);
        detailsLabel.setText ("Changes made here are applied to all loaded sounds.", juce::dontSendNotification);
        // Show the values of the first sound as reference (or the defaults if there are no sounds)
        auto firstSound = sampler.state.getChildWithName (SourceIDs::PRESET).getChildWithName (SourceIDs::SOUND);
        for (auto* control : parameterControls)
            if (! control->isBeingEdited())
                control->setValue (firstSound.isValid() ? (float) firstSound.getProperty (control->getInfo().name, control->getInfo().defaultValue)
                                                        : control->getInfo().defaultValue);
        return;
    }

    // Title and details
    juce::ValueTree firstSample = soundState.getChildWithName (SourceIDs::SOUND_SAMPLE);
    titleLabel.setText (firstSample.getProperty (SourceIDs::name, "(no name)").toString(), juce::dontSendNotification);
    juce::StringArray details;
    if (firstSample.getProperty (SourceIDs::username).toString().isNotEmpty())
        details.add ("by " + firstSample.getProperty (SourceIDs::username).toString());
    if ((int) firstSample.getProperty (SourceIDs::soundId, -1) > -1)
        details.add ("Freesound #" + firstSample.getProperty (SourceIDs::soundId).toString());
    const float duration = firstSample.getProperty (SourceIDs::duration, -1.0f);
    if (duration > 0.0f)
        details.add (juce::String (duration, 2) + "s");
    if (firstSample.getProperty (SourceIDs::license).toString().isNotEmpty())
        details.add (getShortLicenseName (firstSample.getProperty (SourceIDs::license).toString()));
    if ((bool) firstSample.getProperty (SourceIDs::usesPreview, false))
        details.add ("preview quality");
    detailsLabel.setText (details.joinIntoString ("  |  "), juce::dontSendNotification);

    // Parameter values
    for (auto* control : parameterControls)
        if (! control->isBeingEdited())
            control->setValue ((float) soundState.getProperty (control->getInfo().name, control->getInfo().defaultValue));

    if (! rootNoteSlider.isMouseButtonDown())
        rootNoteSlider.setValue ((int) firstSample.getProperty (SourceIDs::midiRootNote, SourceDefaults::midiRootNote), juce::dontSendNotification);

    // Waveform (only reload if the loaded sample changed)
    const bool loaded = soundState.getProperty (SourceIDs::allSoundsLoaded, false);
    const juce::String sampleUUID = firstSample.getProperty (SourceIDs::uuid).toString();
    if (loaded && sampleUUID != loadedWaveformForSampleUUID)
    {
        if (auto* sound = sampler.getSoundWithUUID (soundUUID))
        {
            if (auto* samplerSound = sound->getFirstLinkedSourceSamplerSound())
            {
                waveform.setAudio (samplerSound->getAudioData());
                loadedWaveformForSampleUUID = sampleUUID;
            }
        }
    }
    if (! waveform.hasAudio())
        waveform.setMessage (loaded ? "No audio" : "Loading " + juce::String ((float) firstSample.getProperty (SourceIDs::downloadProgress, 0.0f), 0) + "%...");

    const int launchMode = soundState.getProperty (SourceIDs::launchMode, 0);
    waveform.setMarkers (soundState.getProperty (SourceIDs::startPosition, 0.0f), soundState.getProperty (SourceIDs::endPosition, 1.0f),
                         soundState.getProperty (SourceIDs::loopStartPosition, 0.0f), soundState.getProperty (SourceIDs::loopEndPosition, 1.0f),
                         launchMode == LAUNCH_MODE_LOOP || launchMode == LAUNCH_MODE_LOOP_FW_BW);

    midiMappings.updateFromState (soundUUID, soundState);
    mpeMappings.updateFromState (soundUUID, soundState, sampler.state.getProperty (SourceIDs::mpeEnabled, false));
    if (midiMappings.getIdealHeight() != midiMappings.getHeight() || mpeMappings.getIdealHeight() != mpeMappings.getHeight())
        layoutParameters();
}

void SoundEditorComponent::paint (juce::Graphics& g)
{
    g.setColour (SourceColours::card);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 5.0f);
}

void SoundEditorComponent::resized()
{
    auto b = getLocalBounds().reduced (10);
    titleLabel.setBounds (b.removeFromTop (24));
    detailsLabel.setBounds (b.removeFromTop (18));
    b.removeFromTop (6);
    if (soundUUID.isNotEmpty())
    {
        waveform.setBounds (b.removeFromTop (110));
        b.removeFromTop (6);
        auto rootRow = b.removeFromTop (22);
        rootNoteLabel.setBounds (rootRow.removeFromLeft (96));
        rootNoteSlider.setBounds (rootRow.removeFromLeft (130));
        b.removeFromTop (6);
    }
    parametersViewport.setBounds (b);
    layoutParameters();
}

void SoundEditorComponent::layoutParameters()
{
    // Groups are laid out in columns (as many as fit), each control is a row
    const int width = parametersViewport.getWidth() - parametersViewport.getScrollBarThickness() - 4;
    const int minColumnWidth = 300;
    const int numColumns = juce::jmax (1, width / minColumnWidth);
    const int columnWidth = width / numColumns;
    const int rowHeight = 24;
    const int headerHeight = 22;

    std::vector<int> columnHeights ((size_t) numColumns, 0);
    for (auto& group : groups)
    {
        // Put each group in the shortest column
        auto column = (size_t) std::distance (columnHeights.begin(), std::min_element (columnHeights.begin(), columnHeights.end()));
        int x = (int) column * columnWidth;
        int y = columnHeights[column];
        group.header->setBounds (x, y, columnWidth - 10, headerHeight);
        y += headerHeight;
        for (auto* control : group.controls)
        {
            control->setBounds (x, y, columnWidth - 12, rowHeight);
            y += rowHeight;
        }
        columnHeights[column] = y + 10;
    }
    int contentHeight = *std::max_element (columnHeights.begin(), columnHeights.end());
    if (mpeMappings.isVisible())
    {
        mpeMappings.setBounds (0, contentHeight, width - 12, mpeMappings.getIdealHeight());
        contentHeight += mpeMappings.getIdealHeight() + 6;
    }
    if (midiMappings.isVisible())
    {
        midiMappings.setBounds (0, contentHeight, width - 12, midiMappings.getIdealHeight());
        contentHeight += midiMappings.getIdealHeight();
    }
    parametersContent.setSize (width, contentHeight + 10);
}
