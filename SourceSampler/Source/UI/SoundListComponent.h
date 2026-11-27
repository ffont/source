/*
  ==============================================================================

    SoundListComponent.h

    List of the sounds loaded in the sampler. The first row ("All sounds") is
    used to edit the parameters of all sounds at once.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "SourceLookAndFeel.h"


/** A snapshot of the information about a sound that the list needs (taken from the state). */
struct SoundListItem
{
    juce::String uuid;
    juce::String name;
    juce::String username;
    juce::String license;
    int soundId = -1;
    float downloadProgress = 0.0f;  // 0-100 (failed downloads count as finished)
    int numFailedDownloads = 0;
    int numMissingLocalFiles = 0;     // Samples using local files which are not in this device
    bool downloaded = false;          // All files are available (downloaded or local)
    juce::int64 downloadSizeBytes = -1;  // Size of the downloaded files (-1 if unknown)
    bool loaded = false;
    bool willBeDeleted = false;
    bool fromFreesound = false;       // (first sample) can be replaced by similar sounds
    bool findingSimilar = false;
    int numSamples = 0;
    int rootNote = -1;
    int numMappedNotes = 0;
    juce::String notesDescription;

    static SoundListItem fromState (const juce::ValueTree& soundState);
};


class SoundListComponent : public juce::Component,
                           private juce::ListBoxModel
{
public:
    explicit SoundListComponent (SourceActionSender actionSender);
    ~SoundListComponent() override;

    /** Refreshes the list from the PRESET ValueTree. */
    void updateFromState (const juce::ValueTree& presetState);

    /** Number of active voices for each sound UUID (to highlight sounds being played). */
    void setActiveVoices (const std::map<juce::String, int>& activeVoicesPerSound);

    /** UUID of the selected sound, or empty if "All sounds" is selected. */
    juce::String getSelectedSoundUUID() const;
    void selectSound (const juce::String& uuid);
    int getNumSounds() const { return (int) items.size(); }
    const std::vector<SoundListItem>& getItems() const { return items; }

    std::function<void (const juce::String& selectedUUID)> onSelectionChanged;

    void resized() override;
    void paint (juce::Graphics& g) override;

private:
    class RowComponent;

    int getNumRows() override { return (int) items.size() + 1; }
    void paintListBoxItem (int, juce::Graphics&, int, int, bool) override {}
    juce::Component* refreshComponentForRow (int rowNumber, bool isRowSelected, juce::Component* existingComponentToUpdate) override;
    void selectedRowsChanged (int lastRowSelected) override;

    SourceActionSender actions;
    std::vector<SoundListItem> items;
    std::map<juce::String, int> activeVoices;
    juce::ListBox listBox { "Sounds", this };
    juce::TextButton clearAllButton { "Clear all sounds" };
    juce::Label emptyLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoundListComponent)
};
