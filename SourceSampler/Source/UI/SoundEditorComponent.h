/*
  ==============================================================================

    SoundEditorComponent.h

    Editor for the selected sound (or for all sounds): waveform with start/end/
    loop markers and playheads, all sound parameters, root note and MIDI CC
    mappings.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "SourceLookAndFeel.h"
#include "SourceParameters.h"


class SourceSampler;


/** Waveform overview of a sound with start/end/loop markers and playhead positions. */
class WaveformComponent : public juce::Component,
                          public juce::SettableTooltipClient
{
public:
    /** Computes the min/max peaks of the given audio (call from the message thread). */
    void setAudio (const juce::AudioBuffer<float>* audio);
    bool hasAudio() const { return ! peaks.empty(); }

    void setMarkers (float start, float end, float loopStart, float loopEnd, bool showLoop);
    void setPlayheads (std::vector<float> positions);
    void setMessage (const juce::String& message) { messageText = message; repaint(); }

    void paint (juce::Graphics& g) override;

    std::function<void()> onClick;
    void mouseDown (const juce::MouseEvent&) override { if (onClick) onClick(); }

private:
    static constexpr int numPeakBins = 2048;
    std::vector<std::pair<float, float>> peaks;  // Computed once (the audio is not referenced afterwards)
    float startPos = 0.0f, endPos = 1.0f, loopStartPos = 0.0f, loopEndPos = 1.0f;
    bool loopVisible = false;
    std::vector<float> playheads;
    juce::String messageText;
};


/** One parameter control: label + slider (or combo box for enumerations, or toggle for booleans). */
class ParameterControl : public juce::Component
{
public:
    ParameterControl (const SoundParameterInfo& info, std::function<void (const SoundParameterInfo&, float)> onChange);

    void setValue (float value);  // Does not trigger onChange
    const SoundParameterInfo& getInfo() const { return info; }
    bool isBeingEdited() const;

    void resized() override;

private:
    const SoundParameterInfo& info;
    std::function<void (const SoundParameterInfo&, float)> onChange;
    juce::Label label;
    std::unique_ptr<juce::Slider> slider;
    std::unique_ptr<juce::ComboBox> comboBox;
    std::unique_ptr<juce::ToggleButton> toggle;
};


/** Editor of the MIDI CC mappings of a sound. */
class MidiMappingsComponent : public juce::Component
{
public:
    explicit MidiMappingsComponent (SourceActionSender actionSender);

    void updateFromState (const juce::String& soundUUID, const juce::ValueTree& soundState);
    int getIdealHeight() const;
    void resized() override;
    void paint (juce::Graphics& g) override;

private:
    struct MappingRow : public juce::Component
    {
        MappingRow (MidiMappingsComponent& owner);
        void resized() override;
        juce::String uuid;
        juce::Slider ccNumber;
        juce::ComboBox parameter;
        juce::Slider minRange, maxRange;
        IconButton removeButton { "Remove mapping", IconButton::Icon::remove };
        juce::TextButton saveButton { "Save" };
    };

    void sendMapping (MappingRow& row);
    SourceActionSender actions;
    juce::String currentSoundUUID;
    juce::OwnedArray<MappingRow> rows;
    juce::TextButton addButton { "Add CC mapping" };
    juce::Label lastCCLabel;
    friend struct MappingRow;
};


/** Editor of the MPE mappings of a sound (per-note expression -> sound parameter). */
class MpeMappingsComponent : public juce::Component
{
public:
    explicit MpeMappingsComponent (SourceActionSender actionSender);

    void updateFromState (const juce::String& soundUUID, const juce::ValueTree& soundState, bool mpeEnabled);
    int getIdealHeight() const;
    void resized() override;
    void paint (juce::Graphics& g) override;

private:
    struct MappingRow : public juce::Component
    {
        explicit MappingRow (MpeMappingsComponent& owner);
        void resized() override;
        bool isBeingEdited() const;
        juce::String uuid;
        juce::ComboBox source, parameter;
        juce::Slider minRange, maxRange, curve;
        IconButton removeButton { "Remove mapping", IconButton::Icon::remove };
    };

    void sendMapping (MappingRow& row);
    SourceActionSender actions;
    juce::String currentSoundUUID;
    bool isMpeEnabled = false;
    juce::OwnedArray<MappingRow> rows;
    juce::TextButton addButton { "Add MPE mapping" };
    juce::Label columnsLabel;
};


class SoundEditorComponent : public juce::Component
{
public:
    SoundEditorComponent (SourceSampler& sampler, SourceActionSender actionSender);

    /** Shows the sound with the given UUID, or "All sounds" if the UUID is empty. */
    void setSound (const juce::String& soundUUID);
    const juce::String& getSoundUUID() const { return soundUUID; }

    /** Refreshes all values from the state. */
    void updateFromState();
    void setPlayheads (std::vector<float> positions) { waveform.setPlayheads (std::move (positions)); }

    void paint (juce::Graphics& g) override;
    void resized() override;

    /** Renders the whole parameters panel (including the parts hidden by scrolling). Used for debugging. */
    juce::Image createParametersSnapshot() { return parametersContent.createComponentSnapshot (parametersContent.getLocalBounds()); }

private:
    juce::ValueTree getSoundState() const;
    void parameterChanged (const SoundParameterInfo& info, float value);
    void layoutParameters();

    SourceSampler& sampler;
    SourceActionSender actions;
    juce::String soundUUID;
    juce::String loadedWaveformForSampleUUID;

    juce::Label titleLabel, detailsLabel;
    WaveformComponent waveform;
    juce::Slider rootNoteSlider;
    juce::Label rootNoteLabel;

    struct Group
    {
        juce::String name;
        std::vector<ParameterControl*> controls;
        std::unique_ptr<juce::Label> header;
    };
    juce::Component parametersContent;
    juce::Viewport parametersViewport;
    juce::OwnedArray<ParameterControl> parameterControls;
    std::vector<Group> groups;
    MidiMappingsComponent midiMappings;
    MpeMappingsComponent mpeMappings;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoundEditorComponent)
};
