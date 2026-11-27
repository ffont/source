/*
  ==============================================================================

    SearchPanel.h

    "Search" tab of the native UI: searches Freesound (with license and
    duration filters) and shows the results in a table or in a map (see
    SoundMapComponent). Selecting a result plays its preview and shows it in the
    bar at the bottom, from where it can be added to the preset or used to
    replace the audio of one of the sounds of the preset.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "SourceLookAndFeel.h"
#include "SoundMapComponent.h"
#include "FreesoundSearch.h"
#include "AcousticFilterComponent.h"


class SearchPanel : public juce::Component,
                    private juce::ChangeListener,
                    private juce::TableListBoxModel
{
public:
    SearchPanel (FreesoundSearch& search, SourceActionSender actionSender, std::function<juce::ValueTree()> presetGetter);
    ~SearchPanel() override;

    /** Updates which results are already in the preset (message thread). */
    void updateFromState (const juce::ValueTree& presetState);
    /** Shows the state of the preview of the selected sound (loading, playing, failed). */
    void setPreviewState (int state);  // SourceSampler::PreviewState

    void resized() override;

    int getSelectedIndex() const { return selectedIndex; }
    void selectResult (int index, bool play);
    void addResultToPreset (int index);
    /** Replaces the audio of a sound of the preset with a result (keeping the settings of the sound). */
    void replaceSoundWithResult (const juce::String& soundUUID, int index);
    void runSearch();
    void showMap (bool shouldShowMap);
    SoundMapComponent& getMap() { return map; }
    juce::TableListBox& getTable() { return table; }

    enum Column { colourColumn = 1, nameColumn, userColumn, durationColumn, licenseColumn, formatColumn, sizeColumn, addColumn };

private:
    class AddButtonCell;

    // ChangeListener (search status/results)
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    // TableListBoxModel
    int getNumRows() override;
    void paintRowBackground (juce::Graphics&, int row, int width, int height, bool selected) override;
    void paintCell (juce::Graphics&, int row, int columnId, int width, int height, bool selected) override;
    juce::Component* refreshComponentForCell (int row, int columnId, bool selected, juce::Component* existing) override;
    void cellClicked (int row, int columnId, const juce::MouseEvent&) override;
    void sortOrderChanged (int columnId, bool forwards) override;

    void updateRowOrder();
    void updateSelectedSoundInfo();
    void showReplaceMenu();
    const FreesoundSearchResult* getResult (int index) const;
    int rowToIndex (int row) const;
    int indexToRow (int index) const;

    FreesoundSearch& search;
    SourceActionSender actions;
    std::function<juce::ValueTree()> getPreset;
    std::shared_ptr<const std::vector<FreesoundSearchResult>> results;
    std::vector<int> rowOrder;  // Result index shown in each row of the table (depends on the sorting)
    int sortColumn = 0;
    bool sortForwards = true;
    std::set<int> soundIdsInPreset;
    int selectedIndex = -1;
    bool mapVisible = true;
    int previewState = -1;  // Unknown until the first update

    // Search controls
    CardComponent searchCard;
    juce::TextEditor queryEditor;
    juce::TextButton searchButton { "Search" };
    juce::Label licenseLabel, durationLabel, durationToLabel, maxResultsLabel, statusLabel;
    juce::ComboBox licenseCombo, maxResultsCombo;
    juce::Slider minDurationSlider, maxDurationSlider;
    juce::TextButton listViewButton { "List" }, mapViewButton { "Map" };
    juce::OwnedArray<AcousticFilterComponent> acousticFilters;
    int layoutAcousticFilters (juce::Rectangle<int> area, bool apply);  // Returns the height used

    // Results
    juce::TableListBox table;
    SoundMapComponent map;

    // Selected sound
    CardComponent selectedCard;
    juce::Label selectedNameLabel, selectedDetailsLabel, previewStatusLabel;
    IconButton playButton { "Play", IconButton::Icon::play };
    IconButton stopButton { "Stop", IconButton::Icon::stop };
    juce::TextButton addButton { "Add to preset" };
    juce::TextButton replaceButton { "Replace sound..." };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SearchPanel)
};
