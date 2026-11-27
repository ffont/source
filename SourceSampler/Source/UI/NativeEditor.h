/*
  ==============================================================================

    NativeEditor.h

    Native JUCE editor for SOURCE. It reads the sampler state (ValueTree)
    directly and controls the sampler through the same action messages used by
    the other UIs (see defines_source.h).

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "SourceLookAndFeel.h"
#include "SoundListComponent.h"
#include "SoundEditorComponent.h"


/** Shows the RMS level of each output channel. */
class LevelMetersComponent : public juce::Component
{
public:
    void setLevels (const std::vector<float>& newLevels);
    void paint (juce::Graphics& g) override;

private:
    std::vector<float> levels;
};


/** Shows which voices are active. */
class VoicesComponent : public juce::Component
{
public:
    void setVoices (const std::vector<SourceSampler::VoiceInfo>& voices);
    void paint (juce::Graphics& g) override;

private:
    std::vector<bool> activeVoices;
};


/** Combo box that asks for its items to be refreshed right before showing its popup. */
class PresetSlotsComboBox : public juce::ComboBox
{
public:
    std::function<void()> onPopupOpening;
    void showPopup() override
    {
        if (onPopupOpening != nullptr)
            onPopupOpening();
        juce::ComboBox::showPopup();
    }
};


class SourceNativeEditor : public juce::AudioProcessorEditor,
                           public juce::FileDragAndDropTarget,
                           private juce::ValueTree::Listener,
                           private juce::Timer
{
public:
    explicit SourceNativeEditor (SourceSamplerAudioProcessor&);
    ~SourceNativeEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { isDraggingFiles = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { isDraggingFiles = false; repaint(); }
    void filesDropped (const juce::StringArray& files, int x, int y) override;

private:
    void sendAction (const juce::String& actionName, const juce::StringArray& parameters = {});
    void updateFromState();
    void timerCallback() override;
    void sendReverbParameters();
    void runQuery (const juce::String& queryActionName);
    juce::ValueTree getPresetState() const { return sourceSampler.state.getChildWithName (SourceIDs::PRESET); }
    void layoutContent (juce::Rectangle<int> bounds, bool portrait);

    // Preset slots (0-127, the file of slot N is "N.xml" in the presets folder)
    void refreshPresetSlots();
    juce::String getPresetSlotText (int index) const;
    void showSaveToMenu();
    void savePresetTo (int index);
    std::array<juce::String, NUM_PRESET_SLOTS> presetSlotNames;
    std::array<juce::Time, NUM_PRESET_SLOTS> presetSlotModificationTimes;
    std::array<bool, NUM_PRESET_SLOTS> presetSlotUsed {};
    int lastPresetIndexShown = -2;
    juce::String lastPresetNameShown;

    // ValueTree::Listener: changes are coalesced and the UI is refreshed in the timer callback
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override { stateDirty = true; }
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override { stateDirty = true; }
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override { stateDirty = true; }
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override { stateDirty = true; }
    void valueTreeRedirected (juce::ValueTree&) override { stateDirty = true; }

    SourceSamplerAudioProcessor& audioProcessor;
    SourceSampler& sourceSampler;
    SourceLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltipWindow { this, 600 };
    SourceActionSender actions;
    bool stateDirty = true;
    
    // All the UI lives in this component, which is laid out at a minimum logical size and scaled down to fit
    // small screens (iPad, AUv3 hosts with small windows)
    juce::Component content;
    bool isDraggingFiles = false;

    // Header
    juce::ImageComponent titleImage, logoImage;
    juce::Label statusLabel;

    // Preset & settings
    CardComponent presetCard { "Preset & settings" };
    juce::Label presetLabel, presetNumberLabel, midiChannelLabel, numVoicesLabel;
    PresetSlotsComboBox presetSlotsCombo;
    juce::TextEditor presetNameEditor;
    juce::TextButton savePresetButton { "Save" }, saveToPresetButton { "Save to..." };
    IconButton previousPresetButton { "Previous preset", IconButton::Icon::previous };
    IconButton nextPresetButton { "Next preset", IconButton::Icon::next };
    juce::ComboBox midiChannelCombo;
    juce::Slider numVoicesSlider;
    juce::ToggleButton mpeToggle { "MPE" };
    juce::Label mpeBendLabel;
    juce::Slider mpeBendRangeSlider;
    VoicesComponent voicesComponent;

    // Get new sounds
    CardComponent querySoundsCard { "Get new sounds" };
    juce::Label queryLabel, numSoundsLabel, lengthLabel, lengthToLabel, layoutLabel;
    juce::TextEditor queryEditor;
    juce::Slider numSoundsSlider, minLengthSlider, maxLengthSlider, soundToReplaceSlider;
    juce::ComboBox noteLayoutCombo;
    juce::TextButton replaceAllButton { "Search and replace all sounds" };
    juce::TextButton replaceOneButton { "Search and replace sound #" };
    juce::TextButton addSoundsButton { "Search and add sounds" };

    // Reverb
    CardComponent reverbCard { "Reverb" };
    struct ReverbControl
    {
        juce::Identifier id;
        juce::Label label;
        juce::Slider slider;
    };
    std::array<ReverbControl, 6> reverbControls;

    // Meters
    CardComponent metersCard { "Output" };
    LevelMetersComponent levelMeters;

    // Sounds
    SoundListComponent soundList;
    SoundEditorComponent soundEditor;

    // Keyboard
    juce::MidiKeyboardComponent keyboard;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SourceNativeEditor)
};
