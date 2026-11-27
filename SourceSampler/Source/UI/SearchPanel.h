/*
  ==============================================================================

    SearchPanel.h

    "Search" tab of the native UI: searches Freesound (with license and
    duration filters) and shows the results in a list or in a map (see
    SoundMapComponent). Clicking a result plays its preview and shows it in the
    bar at the bottom, from where it can be added to the preset.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "SourceLookAndFeel.h"
#include "SoundMapComponent.h"
#include "FreesoundSearch.h"


class SearchPanel : public juce::Component,
                    private juce::ChangeListener,
                    private juce::ListBoxModel
{
public:
    SearchPanel (FreesoundSearch& search, SourceActionSender actionSender, std::function<juce::ValueTree()> presetGetter);
    ~SearchPanel() override;

    /** Updates which results are already in the preset (message thread). */
    void updateFromState (const juce::ValueTree& presetState);
    void setPreviewPlaying (bool isPlaying);

    void resized() override;
    void paint (juce::Graphics& g) override;

    int getSelectedIndex() const { return selectedIndex; }
    void selectResult (int index, bool play);
    void addResultToPreset (int index);
    void runSearch();
    void showMap (bool shouldShowMap);
    SoundMapComponent& getMap() { return map; }

private:
    class ResultRow;

    // ChangeListener (search status/results)
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    // ListBoxModel
    int getNumRows() override;
    void paintListBoxItem (int, juce::Graphics&, int, int, bool) override {}
    juce::Component* refreshComponentForRow (int row, bool isSelected, juce::Component* existing) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;

    void updateSelectedSoundInfo();
    const FreesoundSearchResult* getResult (int index) const;
    juce::String describe (const FreesoundSearchResult& result) const;

    FreesoundSearch& search;
    SourceActionSender actions;
    std::function<juce::ValueTree()> getPreset;
    std::shared_ptr<const std::vector<FreesoundSearchResult>> results;
    std::set<int> soundIdsInPreset;
    int selectedIndex = -1;
    bool mapVisible = true;

    // Search controls
    CardComponent searchCard;
    juce::TextEditor queryEditor;
    juce::TextButton searchButton { "Search" };
    juce::Label licenseLabel, durationLabel, durationToLabel, maxResultsLabel, statusLabel;
    juce::ComboBox licenseCombo, maxResultsCombo;
    juce::Slider minDurationSlider, maxDurationSlider;
    juce::TextButton listViewButton { "List" }, mapViewButton { "Map" };

    // Results
    juce::ListBox listBox;
    SoundMapComponent map;

    // Selected sound
    CardComponent selectedCard;
    juce::Label selectedNameLabel, selectedDetailsLabel;
    IconButton playButton { "Play", IconButton::Icon::play };
    IconButton stopButton { "Stop", IconButton::Icon::stop };
    juce::TextButton addButton { "Add to preset" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SearchPanel)
};
