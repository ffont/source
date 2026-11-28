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
#include "SoundGraphs.h"
#include "SourceParameters.h"
#include <deque>
#include <map>


class SourceSampler;


/** Waveform of a sound with start/end/loop markers, slice markers and playhead positions. It can be zoomed (mouse
    wheel or trackpad scroll and pinch, two-finger pinch on touch screens; double-click resets the zoom) and moved when
    zoomed (dragging, horizontal scroll). Editable slice markers can be dragged by their handle (and removed with a
    double-click on the handle). */
class WaveformComponent : public juce::Component,
                          public juce::SettableTooltipClient
{
public:
    /** Computes the min/max peaks of the given audio (call from the message thread). */
    void setAudio (const juce::AudioBuffer<float>* audio, double sampleRate);
    bool hasAudio() const { return ! peaks.empty(); }
    double getDuration() const { return duration; }

    void setMarkers (float start, float end, float loopStart, float loopEnd, bool showLoop);
    void setPlayheads (std::vector<float> positions);
    void setMessage (const juce::String& message) { messageText = message; repaint(); }
    /** Slice markers (positions 0..1). Editable ones can be dragged; inactive ones are drawn faded. */
    void setSlices (std::vector<float> positions, bool editable, bool active);
    /** Called when the user moves or removes editable slice markers (positions 0..1, sorted). */
    std::function<void (const std::vector<float>&)> onSlicesChanged;
    /** Called when the user drags the start/end or loop start/end markers (the name of the parameter and its new value, 0..1). */
    std::function<void (const juce::String& parameterName, float value)> onMarkerChanged;
    /** Part of the sound (0..1) shown. */
    juce::Range<float> getVisibleRange() const { return { viewStart, viewEnd }; }
    void resetZoom();

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    void mouseMagnify (const juce::MouseEvent& e, float scaleFactor) override;

private:
    float xForPosition (float position) const;
    float positionForX (float x) const;
    void setView (float start, float end);
    void zoomAround (float factor, float anchorX);  // factor > 1 zooms in
    int findSliceAt (juce::Point<float> point) const;  // Index of the editable slice marker at the point, or -1
    enum class Marker { none, start, end, loopStart, loopEnd };
    Marker findMarkerAt (juce::Point<float> point) const;  // Start/end (top handles) or loop (bottom handles) marker at the point
    float getMarkerPosition (Marker marker) const;
    void setMarkerPosition (Marker marker, float position);  // Keeps start < end and loop start < loop end, notifies onMarkerChanged
    static constexpr float handleSize = 16.0f;

    static constexpr int numPeakBins = 65536;
    std::vector<std::pair<float, float>> peaks;  // Computed once (the audio is not referenced afterwards)
    double duration = 0.0;
    float startPos = 0.0f, endPos = 1.0f, loopStartPos = 0.0f, loopEndPos = 1.0f;
    bool loopVisible = false;
    std::vector<float> playheads;
    juce::String messageText;
    std::vector<float> slices;
    bool slicesEditable = false, slicesActive = false;
    float viewStart = 0.0f, viewEnd = 1.0f;

    // Gestures
    std::map<int, juce::Point<float>> touches;  // Position of each finger/mouse (by input source index)
    int draggedSlice = -1;
    Marker draggedMarker = Marker::none;
    bool pinching = false;
    float pinchStartDistance = 1.0f, pinchAnchorPosition = 0.0f;
    juce::Range<float> gestureStartView;
    float panStartX = 0.0f;
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


/** Small oscilloscope that shows the output of a modulator over the last seconds, one trace per voice. */
class ModulatorScope : public juce::Component,
                       public juce::SettableTooltipClient
{
public:
    void setRange (float newMinRange, float newMaxRange);
    /** Vertical scale (default -1..1, the offsets of modulators; mappings use 0..1, normalised parameter values). */
    void setVerticalRange (float bottom, float top) { verticalBottom = bottom; verticalTop = top; repaint(); }
    /** Text drawn in the scope (e.g. what the row does, or a warning). */
    void setMessage (const juce::String& text, bool isWarning);
    /** Adds the newest values of a voice (spaced 1 / rate seconds, the last one at time now). A change of noteCounter
        starts a new segment of the voice's trace. */
    void addValues (int voiceIndex, uint32_t noteCounter, const std::vector<float>& values, double rate, double now);
    /** Drops the points that are out of the window and repaints. */
    void advanceTime (double now);
    void paint (juce::Graphics& g) override;

    static juce::Colour getVoiceColour (int voiceIndex);
    static constexpr double windowSeconds = 2.0;

private:
    struct Trace
    {
        uint32_t noteCounter = 0;
        std::deque<std::pair<double, float>> points;  // (time, value), NaN values break the line
    };
    std::map<int, Trace> traces;
    float minRange = 0.0f, maxRange = 0.0f;
    float verticalBottom = -1.0f, verticalTop = 1.0f;
    juce::String message;
    bool messageIsWarning = false;
    double currentTime = 0.0;
};


/** Bar plot of the values of a steps modulator: drag on it to draw the values. */
class StepsEditor : public juce::Component,
                    public juce::SettableTooltipClient
{
public:
    /** Sets the values and number of steps shown (not while the user is dragging). */
    void setValues (const SourceHelpers::StepValues& newValues, int newNumSteps);
    const SourceHelpers::StepValues& getValues() const { return values; }
    bool isDragging() const { return dragging; }
    /** Called while dragging, when the values change. */
    std::function<void()> onChange;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;

private:
    juce::Rectangle<float> getPlotArea() const { return getLocalBounds().toFloat().reduced (2.0f, 3.0f); }
    void setValueAt (juce::Point<float> position);
    SourceHelpers::StepValues values = SourceHelpers::getDefaultStepValues();
    int numSteps = SourceDefaults::numSteps;
    bool dragging = false;
    int lastIndex = -1;
    float lastValue = 0.0f;
};


/** Editor of the modulators of a sound (internal LFO/ADSR signals that modulate a parameter, per voice), each with an
    oscilloscope showing its output for the voices playing the sound. */
class ModulatorsComponent : public juce::Component
{
public:
    explicit ModulatorsComponent (SourceActionSender actionSender);

    /** Shows the modulators, MPE mappings and MIDI CC mappings of the sound (in the order they were added). */
    void updateFromState (const juce::String& soundUUID, const juce::ValueTree& soundState, bool mpeEnabled);
    /** Reads the new modulator values of the voices playing the sound (call it periodically). */
    void updateScopes (SourceSampler& sampler);
    int getIdealHeight() const;
    void resized() override;
    void paint (juce::Graphics& g) override;

private:
    struct LabelledSlider : public juce::Component
    {
        explicit LabelledSlider (const juce::String& name);
        void resized() override;
        juce::Label label;
        juce::Slider slider;
    };

    struct LabelledCombo : public juce::Component
    {
        explicit LabelledCombo (const juce::String& name);
        void resized() override;
        juce::Label label;
        juce::ComboBox combo;
    };

    /** A row of the list: a modulator, an MPE mapping or a MIDI CC mapping. */
    struct Row : public juce::Component
    {
        juce::Identifier kind;  // SourceIDs::MODULATOR, MPE_MAPPING or MIDI_CC_MAPPING
        juce::String uuid;
        juce::Label typeLabel;
        juce::ComboBox target;
        LabelledSlider minRange { "Min" }, maxRange { "Max" };
        IconButton removeButton { "Remove", IconButton::Icon::remove };
        virtual int getPreferredHeight() const = 0;
        virtual bool isBeingEdited() const = 0;
        void paint (juce::Graphics& g) override;
        static constexpr int typeLabelWidth = 72;
    };

    struct ModulatorRow : public Row
    {
        ModulatorRow (ModulatorsComponent& owner, SourceHelpers::ModulatorType type);
        void updateSyncVisibility();
        bool isSourceType() const { return type == SourceHelpers::ModulatorType::aftertouch || type == SourceHelpers::ModulatorType::velocity; }
        int getPreferredHeight() const override { return type == SourceHelpers::ModulatorType::steps ? 150 : (isSourceType() ? 62 : 92); }
        void resized() override;
        bool isBeingEdited() const override;
        SourceHelpers::ModulatorType type;
        juce::ComboBox waveform;
        LabelledSlider rate { "Rate" }, lfoAttack { "Attack" };
        LabelledCombo rateDivision { "Rate" }, attackDivision { "Attack" };
        juce::TextButton syncButton { "Sync" };
        LabelledSlider numSteps { "Steps" }, stepsRate { "Rate" };
        LabelledCombo stepsLength { "Cycle" };
        StepsEditor stepsEditor;
        LabelledSlider attack { "A" }, decay { "D" }, sustain { "S" }, release { "R" };
        ModulatorScope scope;
    };

    /** MPE mapping (per-note expression -> parameter value of that note) or MIDI CC mapping (CC -> the sound's
        parameter value, for all notes). Min and max are normalised parameter values. */
    struct MappingRow : public Row
    {
        MappingRow (ModulatorsComponent& owner, bool isMpe);
        int getPreferredHeight() const override { return 62; }
        bool isBeingEdited() const override;
        void resized() override;
        const bool isMpe;
        juce::StringArray parameterNames;  // Target choices (the combo shows their display names)
        juce::ComboBox source;  // MPE
        LabelledSlider curve { "Curve" };  // MPE
        LabelledSlider ccNumber { "CC" };  // MIDI CC
        ModulatorScope scope;
    };

    void addModulator (SourceHelpers::ModulatorType type);
    void addMapping (bool isMpe);
    void sendModulator (ModulatorRow& row);
    void sendMapping (MappingRow& row);
    void updateModulatorRow (ModulatorRow& row, const juce::ValueTree& state);
    void updateMappingRow (MappingRow& row, const juce::ValueTree& state, bool mpeEnabled);
    std::vector<juce::TextButton*> getAddButtons()
    {
        return { &addLfoButton, &addAdsrButton, &addStepsButton, &addAftertouchButton, &addVelocityButton, &addMpeButton, &addCcButton };
    }
    SourceActionSender actions;
    juce::String currentSoundUUID;
    juce::OwnedArray<Row> rows;
    std::vector<ModulatorRow*> modulatorRows;  // In the order of the modulators (the index used by the voices' history)
    std::vector<MappingRow*> mpeRows, ccRows;  // In the order of the MPE / MIDI CC mappings
    juce::TextButton addLfoButton { "Add LFO" }, addAdsrButton { "Add ADSR" }, addStepsButton { "Add steps" };
    juce::TextButton addAftertouchButton { "Add aftertouch" }, addVelocityButton { "Add velocity" };
    juce::TextButton addMpeButton { "Add MPE" }, addCcButton { "Add MIDI CC" };
    std::map<int, uint32_t> lastReadCounts;  // Per voice
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
    /** Updates the modulators' oscilloscopes (call it periodically). */
    void updateModulatorScopes() { if (modulators.isVisible()) modulators.updateScopes (sampler); }

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
    juce::TextButton similarButton { "Replace with similar" };
    juce::TextButton addSliceButton { "Add slice" }, slicesFromOnsetsButton { "Slices from waveform" };
    std::vector<float> manualSliceTimes;  // Current manual slices (seconds)
    void sendManualSlices (const std::vector<float>& times);
    juce::Label rootNoteLabel;

    struct Group
    {
        juce::String name;
        std::vector<ParameterControl*> controls;
        std::unique_ptr<juce::Label> header;
        std::map<juce::String, std::pair<juce::Component*, int>> graphsBefore;  // Graph (and its height) shown above a control
    };
    FilterGraphComponent filterGraph;
    EnvelopeGraphComponent amplitudeEnvelopeGraph, filterEnvelopeGraph;
    void updateGraphs (const juce::ValueTree& soundState);
    juce::Component parametersContent;
    juce::Viewport parametersViewport;
    juce::OwnedArray<ParameterControl> parameterControls;
    std::vector<Group> groups;
    ModulatorsComponent modulators;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoundEditorComponent)
};
