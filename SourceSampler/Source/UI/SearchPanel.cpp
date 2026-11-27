/*
  ==============================================================================

    SearchPanel.cpp

  ==============================================================================
*/

#include "SearchPanel.h"
#include <climits>

namespace
{
    constexpr int rowHeight = 22;
    const juce::StringArray maxResultsOptions { "50", "100", "150", "300", "600" };

    void setupLabel (juce::Label& label, const juce::String& text, juce::Component& parent)
    {
        label.setText (text, juce::dontSendNotification);
        label.setColour (juce::Label::textColourId, SourceColours::dimText);
        label.setFont (juce::Font (juce::FontOptions (12.0f)));
        parent.addAndMakeVisible (label);
    }

    void setupNumberSlider (juce::Slider& slider, double min, double max, double interval, double value, juce::Component& parent)
    {
        slider.setSliderStyle (juce::Slider::IncDecButtons);
        slider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 44, 20);
        slider.setRange (min, max, interval);
        slider.setValue (value, juce::dontSendNotification);
        slider.setIncDecButtonsMode (juce::Slider::incDecButtonsDraggable_Vertical);
        parent.addAndMakeVisible (slider);
    }
}


//==============================================================================
/** A row of the results list: colour (tristimulus), name, details and an "Add" button. */
class SearchPanel::ResultRow : public juce::Component
{
public:
    explicit ResultRow (SearchPanel& o) : owner (o)
    {
        addButton.setTooltip ("Add to preset");
        addButton.onClick = [this] { owner.addResultToPreset (row); };
        addAndMakeVisible (addButton);
        setInterceptsMouseClicks (false, true);  // Clicks on the row go to the list box (selection)
    }

    void update (int newRow, bool selected)
    {
        row = newRow;
        isSelected = selected;
        const auto* result = owner.getResult (row);
        inPreset = result != nullptr && owner.soundIdsInPreset.count (result->id) > 0;
        addButton.setVisible (result != nullptr);
        addButton.setButtonText (inPreset ? "Add again" : "Add");
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto* result = owner.getResult (row);
        if (result == nullptr)
            return;
        auto bounds = getLocalBounds().reduced (0, 1).toFloat();
        g.setColour (isSelected ? SourceColours::control.brighter (0.15f) : SourceColours::card);
        g.fillRoundedRectangle (bounds, 4.0f);
        if (isSelected)
        {
            g.setColour (SourceColours::accent);
            g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);
        }
        auto content = getLocalBounds().reduced (8, 4);
        content.removeFromRight (addButton.getWidth() + 8);
        auto dot = content.removeFromLeft (12).withSizeKeepingCentre (10, 10).toFloat();
        g.setColour (result->getColour());
        g.fillEllipse (dot);
        content.removeFromLeft (8);
        auto top = content.removeFromTop (content.getHeight() / 2);
        g.setColour (SourceColours::text);
        g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
        g.drawText (result->name, top, juce::Justification::centredLeft, true);
        g.setColour (SourceColours::dimText);
        g.setFont (juce::Font (juce::FontOptions (11.0f)));
        g.drawText (owner.describe (*result) + (inPreset ? "  (in preset)" : juce::String()), content, juce::Justification::centredLeft, true);
    }

    void resized() override
    {
        addButton.setBounds (getLocalBounds().reduced (6, 0).removeFromRight (78).withSizeKeepingCentre (78, 24));
    }

private:
    SearchPanel& owner;
    juce::TextButton addButton { "Add" };
    int row = -1;
    bool isSelected = false, inPreset = false;
};


//==============================================================================
SearchPanel::SearchPanel (FreesoundSearch& s, SourceActionSender actionSender, std::function<juce::ValueTree()> presetGetter)
    : search (s), actions (std::move (actionSender)), getPreset (std::move (presetGetter)), searchCard ("Search Freesound"),
      selectedCard ({})
{
    const auto& lastOptions = search.getLastOptions();

    addAndMakeVisible (searchCard);
    queryEditor.setTextToShowWhenEmpty ("Search sounds (e.g. \"piano\", \"metal hit\", \"rain\")", SourceColours::dimText);
    queryEditor.setText (lastOptions.query, false);
    queryEditor.onReturnKey = [this] { runSearch(); };
    searchCard.addAndMakeVisible (queryEditor);
    searchButton.onClick = [this] { runSearch(); };
    searchCard.addAndMakeVisible (searchButton);

    setupLabel (licenseLabel, "License", searchCard);
    licenseCombo.addItem ("Any license", 1);
    licenseCombo.addItem ("CC0 only", 2);
    licenseCombo.addItem ("CC0 or Attribution", 3);
    licenseCombo.addItem ("No NonCommercial", 4);
    licenseCombo.setSelectedId ((int) lastOptions.license + 1, juce::dontSendNotification);
    searchCard.addAndMakeVisible (licenseCombo);

    setupLabel (durationLabel, "Duration (s)", searchCard);
    setupNumberSlider (minDurationSlider, 0.0, 300.0, 0.1, lastOptions.minDuration, searchCard);
    setupLabel (durationToLabel, "to", searchCard);
    durationToLabel.setJustificationType (juce::Justification::centred);
    setupNumberSlider (maxDurationSlider, 0.0, 300.0, 0.1, lastOptions.maxDuration > 0.0f ? lastOptions.maxDuration : 10.0, searchCard);
    maxDurationSlider.setTooltip ("Maximum duration (0 for no maximum)");

    setupLabel (maxResultsLabel, "Results", searchCard);
    for (int i = 0; i < maxResultsOptions.size(); ++i)
        maxResultsCombo.addItem (maxResultsOptions[i], i + 1);
    maxResultsCombo.setSelectedId (juce::jmax (1, maxResultsOptions.indexOf (juce::String (lastOptions.maxResults)) + 1), juce::dontSendNotification);
    maxResultsCombo.setTooltip ("Maximum number of sounds to get (the map takes longer to compute with more sounds)");
    searchCard.addAndMakeVisible (maxResultsCombo);

    for (auto* b : { &listViewButton, &mapViewButton })
    {
        b->setClickingTogglesState (false);
        b->setColour (juce::TextButton::buttonOnColourId, SourceColours::accent.darker (0.3f));
        searchCard.addAndMakeVisible (b);
    }
    listViewButton.onClick = [this] { showMap (false); };
    mapViewButton.onClick = [this] { showMap (true); };

    statusLabel.setColour (juce::Label::textColourId, SourceColours::dimText);
    statusLabel.setJustificationType (juce::Justification::centredRight);
    searchCard.addAndMakeVisible (statusLabel);

    // Results
    listBox.setModel (this);
    listBox.setRowHeight (40);
    listBox.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    addChildComponent (listBox);
    map.onSoundClicked = [this] (int index) { selectResult (index, true); };
    addAndMakeVisible (map);

    // Selected sound
    addAndMakeVisible (selectedCard);
    selectedNameLabel.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
    selectedCard.addAndMakeVisible (selectedNameLabel);
    selectedDetailsLabel.setColour (juce::Label::textColourId, SourceColours::dimText);
    selectedCard.addAndMakeVisible (selectedDetailsLabel);
    playButton.onClick = [this] { selectResult (selectedIndex, true); };
    stopButton.onClick = [this] { actions.send (ACTION_PLAY_SOUND_FILE_FROM_PATH, { "" }); };
    addButton.onClick = [this] { addResultToPreset (selectedIndex); };
    addButton.setColour (juce::TextButton::buttonColourId, SourceColours::accent.darker (0.3f));
    for (auto* c : std::initializer_list<juce::Component*> { &playButton, &stopButton, &addButton })
        selectedCard.addAndMakeVisible (c);

    search.addChangeListener (this);
    changeListenerCallback (nullptr);
    showMap (true);
}

SearchPanel::~SearchPanel()
{
    search.removeChangeListener (this);
}

void SearchPanel::runSearch()
{
    FreesoundSearchOptions options;
    options.query = queryEditor.getText().trim();
    options.license = (FreesoundSearchOptions::License) juce::jmax (0, licenseCombo.getSelectedId() - 1);
    options.minDuration = (float) minDurationSlider.getValue();
    options.maxDuration = (float) maxDurationSlider.getValue();
    options.maxResults = maxResultsOptions[juce::jmax (0, maxResultsCombo.getSelectedId() - 1)].getIntValue();
    search.search (options);
}

void SearchPanel::showMap (bool shouldShowMap)
{
    mapVisible = shouldShowMap;
    map.setVisible (mapVisible);
    listBox.setVisible (! mapVisible);
    mapViewButton.setToggleState (mapVisible, juce::dontSendNotification);
    listViewButton.setToggleState (! mapVisible, juce::dontSendNotification);
    if (! mapVisible && selectedIndex >= 0)
        listBox.scrollToEnsureRowIsOnscreen (selectedIndex);
}

void SearchPanel::changeListenerCallback (juce::ChangeBroadcaster*)
{
    statusLabel.setText (search.getStatusMessage(), juce::dontSendNotification);
    if (! queryEditor.hasKeyboardFocus (true) && queryEditor.getText().trim() != search.getLastOptions().query)
        queryEditor.setText (search.getLastOptions().query, false);
    const bool busy = search.getStatus() == FreesoundSearch::Status::searching || search.getStatus() == FreesoundSearch::Status::projecting;
    statusLabel.setColour (juce::Label::textColourId, search.getStatus() == FreesoundSearch::Status::error ? SourceColours::warning
                                                      : (busy ? SourceColours::accent : SourceColours::dimText));
    auto newResults = search.getResults();
    if (newResults != results)
    {
        results = newResults;
        selectedIndex = -1;
        map.setResults (results);
        listBox.updateContent();
        listBox.deselectAllRows();
        listBox.scrollToEnsureRowIsOnscreen (0);
        updateSelectedSoundInfo();
    }
}

const FreesoundSearchResult* SearchPanel::getResult (int index) const
{
    if (results == nullptr || index < 0 || index >= (int) results->size())
        return nullptr;
    return &(*results)[(size_t) index];
}

juce::String SearchPanel::describe (const FreesoundSearchResult& result) const
{
    juce::StringArray parts;
    parts.add ("by " + result.username);
    parts.add (juce::String (result.duration, result.duration < 10.0 ? 2 : 1) + " s");
    parts.add (SourceHelpers::getShortLicenseName (result.license));
    return parts.joinIntoString (" - ");
}

void SearchPanel::selectResult (int index, bool play)
{
    const auto* result = getResult (index);
    if (result == nullptr)
        return;
    selectedIndex = index;
    map.setSelectedIndex (index);
    listBox.selectRow (index, true, true);
    updateSelectedSoundInfo();
    if (play && result->previewURL.isNotEmpty())
        actions.send (ACTION_PLAY_SOUND_FILE_FROM_PATH, { result->previewURL });
}

void SearchPanel::addResultToPreset (int index)
{
    const auto* result = getResult (index);
    if (result == nullptr)
        return;
    // Parameters of ACTION_ADD_SOUND: soundID, name, user, license, previewURL, localFilePath, format, sizeBytes, slices, midiNotes, midiRootNote
    actions.send (ACTION_ADD_SOUND, { juce::String (result->id), SourceActionSender::sanitize (result->name), SourceActionSender::sanitize (result->username),
                                      result->license, result->previewURL, "", result->format, juce::String ((int) juce::jmin ((juce::int64) INT_MAX, result->filesize)),
                                      "", "", "-1" });
    // Lay out notes for all sounds (including the new one)
    const auto preset = getPreset();
    actions.send (ACTION_REAPPLY_LAYOUT, { juce::String ((int) preset.getProperty (SourceIDs::noteLayoutType, SourceDefaults::noteLayoutType)) });
    statusLabel.setText ("Added \"" + result->name + "\" to the preset", juce::dontSendNotification);
    statusLabel.setColour (juce::Label::textColourId, SourceColours::accent);
    updateFromState (getPreset());
}

void SearchPanel::updateFromState (const juce::ValueTree& presetState)
{
    std::set<int> ids;
    for (const auto& sound : presetState)
        if (sound.hasType (SourceIDs::SOUND))
            for (const auto& sample : sound)
                if (sample.hasType (SourceIDs::SOUND_SAMPLE) && (bool) sample.getProperty (SourceIDs::soundFromFreesound, false))
                    ids.insert ((int) sample.getProperty (SourceIDs::soundId, -1));
    if (ids == soundIdsInPreset)
        return;
    soundIdsInPreset = ids;
    map.setHighlightedIds (ids);
    listBox.updateContent();
    listBox.repaint();
    updateSelectedSoundInfo();
}

void SearchPanel::setPreviewPlaying (bool isPlaying)
{
    stopButton.setEnabled (isPlaying);
}

void SearchPanel::updateSelectedSoundInfo()
{
    const auto* result = getResult (selectedIndex);
    for (auto* c : std::initializer_list<juce::Component*> { &playButton, &stopButton, &addButton })
        c->setVisible (result != nullptr);
    if (result == nullptr)
    {
        selectedNameLabel.setText ({}, juce::dontSendNotification);
        selectedDetailsLabel.setText (results != nullptr && ! results->empty() ? "Click a sound to listen to it" : "", juce::dontSendNotification);
        return;
    }
    selectedNameLabel.setText (result->name, juce::dontSendNotification);
    const bool inPreset = soundIdsInPreset.count (result->id) > 0;
    selectedDetailsLabel.setText (describe (*result) + " - " + result->format + " - " + juce::String ((double) result->filesize / (1024.0 * 1024.0), 2)
                                  + " MB - Freesound #" + juce::String (result->id) + (inPreset ? "  (in preset)" : juce::String()),
                                  juce::dontSendNotification);
    addButton.setButtonText (inPreset ? "Add again" : "Add to preset");
}

//==============================================================================
int SearchPanel::getNumRows()
{
    return results != nullptr ? (int) results->size() : 0;
}

juce::Component* SearchPanel::refreshComponentForRow (int row, bool isSelected, juce::Component* existing)
{
    auto* rowComponent = dynamic_cast<ResultRow*> (existing);
    if (rowComponent == nullptr)
    {
        delete existing;
        rowComponent = new ResultRow (*this);
    }
    rowComponent->update (row, isSelected);
    return rowComponent;
}

void SearchPanel::listBoxItemClicked (int row, const juce::MouseEvent&)
{
    selectResult (row, true);
}

//==============================================================================
void SearchPanel::paint (juce::Graphics&) {}

void SearchPanel::resized()
{
    auto b = getLocalBounds();
    const int gap = 8;

    // Search controls: query on the first row, filters and view buttons on the second
    searchCard.setBounds (b.removeFromTop (8 + rowHeight + 6 + rowHeight + 8 + 26));
    {
        auto c = searchCard.getContentBounds();
        auto row = c.removeFromTop (rowHeight);
        searchButton.setBounds (row.removeFromRight (90));
        row.removeFromRight (6);
        queryEditor.setBounds (row);
        c.removeFromTop (6);
        row = c.removeFromTop (rowHeight);
        licenseLabel.setBounds (row.removeFromLeft (48));
        licenseCombo.setBounds (row.removeFromLeft (150));
        row.removeFromLeft (14);
        durationLabel.setBounds (row.removeFromLeft (72));
        minDurationSlider.setBounds (row.removeFromLeft (92));
        durationToLabel.setBounds (row.removeFromLeft (22));
        maxDurationSlider.setBounds (row.removeFromLeft (92));
        row.removeFromLeft (14);
        maxResultsLabel.setBounds (row.removeFromLeft (50));
        maxResultsCombo.setBounds (row.removeFromLeft (70));
        mapViewButton.setBounds (row.removeFromRight (56));
        row.removeFromRight (2);
        listViewButton.setBounds (row.removeFromRight (56));
        // Status in the title row of the card
        statusLabel.setBounds (searchCard.getLocalBounds().reduced (10, 6).removeFromTop (20).withTrimmedLeft (160));
    }
    b.removeFromTop (gap);

    // Selected sound at the bottom
    selectedCard.setBounds (b.removeFromBottom (58));
    {
        auto c = selectedCard.getLocalBounds().reduced (10, 8);
        addButton.setBounds (c.removeFromRight (120).withSizeKeepingCentre (120, 28));
        c.removeFromRight (8);
        stopButton.setBounds (c.removeFromRight (28).withSizeKeepingCentre (28, 28));
        c.removeFromRight (4);
        playButton.setBounds (c.removeFromRight (28).withSizeKeepingCentre (28, 28));
        c.removeFromRight (10);
        selectedNameLabel.setBounds (c.removeFromTop (c.getHeight() / 2));
        selectedDetailsLabel.setBounds (c);
    }
    b.removeFromBottom (gap);

    listBox.setBounds (b);
    map.setBounds (b);
}
