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

    juce::String formatDuration (double seconds)
    {
        return juce::String (seconds, seconds < 10.0 ? 2 : 1) + " s";
    }

    juce::String formatSize (juce::int64 bytes)
    {
        return bytes >= 1024 * 1024 ? juce::String ((double) bytes / (1024.0 * 1024.0), 1) + " MB"
                                    : juce::String (juce::roundToInt ((double) bytes / 1024.0)) + " KB";
    }
}


//==============================================================================
/** "Add" button in the last column of the table. */
class SearchPanel::AddButtonCell : public juce::Component
{
public:
    explicit AddButtonCell (SearchPanel& o) : owner (o)
    {
        button.setTooltip ("Add to preset");
        button.onClick = [this] { owner.addResultToPreset (index); };
        addAndMakeVisible (button);
    }
    void update (int resultIndex, bool inPreset)
    {
        index = resultIndex;
        button.setButtonText (inPreset ? "Add again" : "Add");
    }
    void resized() override { button.setBounds (getLocalBounds().reduced (4, 3)); }

private:
    SearchPanel& owner;
    juce::TextButton button;
    int index = -1;
};


//==============================================================================
/** Filter for an acoustic property: its name and three squares (low, mid, high). Only one square can be selected;
    clicking the selected one clears the filter. */
class SearchPanel::AcousticFilter : public juce::Component,
                                    public juce::SettableTooltipClient
{
public:
   #if JUCE_IOS
    static constexpr int labelWidth = 66, squareSize = 20, squareGap = 4;  // Bigger for fingers
   #else
    static constexpr int labelWidth = 66, squareSize = 15, squareGap = 3;
   #endif
    static constexpr int preferredWidth = labelWidth + 3 * squareSize + 2 * squareGap;

    explicit AcousticFilter (const juce::String& propertyName) : name (propertyName)
    {
        setTooltip (name.substring (0, 1).toUpperCase() + name.substring (1) + ": low, mid or high (click again to clear)");
    }

    FreesoundSearchOptions::Level getLevel() const { return level; }
    void setLevel (FreesoundSearchOptions::Level newLevel) { level = newLevel; repaint(); }

    void paint (juce::Graphics& g) override
    {
        g.setColour (level == FreesoundSearchOptions::Level::any ? SourceColours::dimText : SourceColours::text);
        g.setFont (juce::Font (juce::FontOptions (12.0f)));
        g.drawText (name.substring (0, 1).toUpperCase() + name.substring (1), getLocalBounds().withWidth (labelWidth), juce::Justification::centredLeft, true);
        for (int i = 0; i < 3; ++i)
        {
            const auto square = getSquare (i).toFloat();
            const bool selected = (int) level == i;
            g.setColour (selected ? SourceColours::accent : SourceColours::control);
            g.fillRoundedRectangle (square, 2.0f);
            // Bars of increasing height show low/mid/high
            g.setColour (selected ? SourceColours::background : SourceColours::dimText);
            const float barHeight = square.getHeight() * (0.3f + 0.25f * (float) i);
            g.fillRect (square.reduced (4.0f, 0.0f).withTrimmedTop (square.getHeight() - barHeight).withTrimmedBottom (2.0f));
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        for (int i = 0; i < 3; ++i)
        {
            if (getSquare (i).expanded (1).contains (e.getPosition()))
            {
                setLevel ((int) level == i ? FreesoundSearchOptions::Level::any : (FreesoundSearchOptions::Level) i);
                if (onChange != nullptr)
                    onChange();
                return;
            }
        }
    }

    std::function<void()> onChange;

private:
    juce::Rectangle<int> getSquare (int i) const
    {
        return { labelWidth + i * (squareSize + squareGap), (getHeight() - squareSize) / 2, squareSize, squareSize };
    }

    juce::String name;
    FreesoundSearchOptions::Level level = FreesoundSearchOptions::Level::any;
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
        b->setColour (juce::TextButton::buttonOnColourId, SourceColours::accent.darker (0.3f));
        searchCard.addAndMakeVisible (b);
    }
    for (int i = 0; i < FreesoundSearchOptions::getAcousticPropertyNames().size(); ++i)
    {
        auto* filter = acousticFilters.add (new AcousticFilter (FreesoundSearchOptions::getAcousticPropertyNames()[i]));
        filter->setLevel (lastOptions.acousticLevels[(size_t) i]);
        searchCard.addAndMakeVisible (filter);
    }
    listViewButton.onClick = [this] { showMap (false); };
    mapViewButton.onClick = [this] { showMap (true); };

    statusLabel.setColour (juce::Label::textColourId, SourceColours::dimText);
    statusLabel.setJustificationType (juce::Justification::centredRight);
    searchCard.addAndMakeVisible (statusLabel);

    // Results table
    auto& header = table.getHeader();
    const int sortable = juce::TableHeaderComponent::defaultFlags;
    header.addColumn ({}, colourColumn, 26, 26, 26, juce::TableHeaderComponent::notResizableOrSortable);
    header.addColumn ("Name", nameColumn, 330, 120, 1000, sortable);
    header.addColumn ("User", userColumn, 150, 60, 400, sortable);
    header.addColumn ("Duration", durationColumn, 80, 60, 120, sortable);
    header.addColumn ("License", licenseColumn, 110, 60, 200, sortable);
    header.addColumn ("Format", formatColumn, 70, 50, 100, sortable);
    header.addColumn ("Size", sizeColumn, 80, 50, 120, sortable);
    header.addColumn ({}, addColumn, 96, 96, 96, juce::TableHeaderComponent::notResizableOrSortable);
    header.setStretchToFitActive (true);
    header.setColour (juce::TableHeaderComponent::backgroundColourId, SourceColours::control);
    header.setColour (juce::TableHeaderComponent::textColourId, SourceColours::text);
    header.setColour (juce::TableHeaderComponent::outlineColourId, SourceColours::background);
    table.setModel (this);
    table.setRowHeight (30);
    table.setColour (juce::ListBox::backgroundColourId, SourceColours::card);
    table.setColour (juce::ListBox::outlineColourId, SourceColours::control);
    table.setOutlineThickness (1);
    addChildComponent (table);
    map.onSoundClicked = [this] (int index) { selectResult (index, true); };
    addAndMakeVisible (map);

    // Selected sound
    addAndMakeVisible (selectedCard);
    selectedNameLabel.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
    selectedCard.addAndMakeVisible (selectedNameLabel);
    selectedDetailsLabel.setColour (juce::Label::textColourId, SourceColours::dimText);
    selectedCard.addAndMakeVisible (selectedDetailsLabel);
    previewStatusLabel.setJustificationType (juce::Justification::centredRight);
    previewStatusLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    selectedCard.addAndMakeVisible (previewStatusLabel);
    playButton.onClick = [this] { selectResult (selectedIndex, true); };
    stopButton.onClick = [this] { actions.send (ACTION_PLAY_SOUND_FILE_FROM_PATH, { "" }); };
    addButton.onClick = [this] { addResultToPreset (selectedIndex); };
    addButton.setColour (juce::TextButton::buttonColourId, SourceColours::accent.darker (0.3f));
    replaceButton.setTooltip ("Replace the audio of a sound of the preset with this one (keeping its settings)");
    replaceButton.onClick = [this] { showReplaceMenu(); };
    for (auto* c : std::initializer_list<juce::Component*> { &playButton, &stopButton, &addButton, &replaceButton })
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
    for (int i = 0; i < acousticFilters.size(); ++i)
        options.acousticLevels[(size_t) i] = acousticFilters[i]->getLevel();
    search.search (options);
}

void SearchPanel::showMap (bool shouldShowMap)
{
    mapVisible = shouldShowMap;
    map.setVisible (mapVisible);
    table.setVisible (! mapVisible);
    mapViewButton.setToggleState (mapVisible, juce::dontSendNotification);
    listViewButton.setToggleState (! mapVisible, juce::dontSendNotification);
    if (! mapVisible && selectedIndex >= 0)
        table.scrollToEnsureRowIsOnscreen (indexToRow (selectedIndex));
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
        updateRowOrder();
        table.updateContent();
        table.deselectAllRows();
        table.scrollToEnsureRowIsOnscreen (0);
        updateSelectedSoundInfo();
    }
}

const FreesoundSearchResult* SearchPanel::getResult (int index) const
{
    if (results == nullptr || index < 0 || index >= (int) results->size())
        return nullptr;
    return &(*results)[(size_t) index];
}

int SearchPanel::rowToIndex (int row) const
{
    return (row >= 0 && row < (int) rowOrder.size()) ? rowOrder[(size_t) row] : -1;
}

int SearchPanel::indexToRow (int index) const
{
    for (size_t row = 0; row < rowOrder.size(); ++row)
        if (rowOrder[row] == index)
            return (int) row;
    return -1;
}

void SearchPanel::updateRowOrder()
{
    rowOrder.clear();
    if (results == nullptr)
        return;
    for (int i = 0; i < (int) results->size(); ++i)
        rowOrder.push_back (i);
    if (sortColumn <= 0)
        return;
    const auto& r = *results;
    std::stable_sort (rowOrder.begin(), rowOrder.end(), [this, &r] (int a, int b)
    {
        const auto& x = r[(size_t) (sortForwards ? a : b)];
        const auto& y = r[(size_t) (sortForwards ? b : a)];
        switch (sortColumn)
        {
            case nameColumn:     return x.name.compareNatural (y.name) < 0;
            case userColumn:     return x.username.compareIgnoreCase (y.username) < 0;
            case durationColumn: return x.duration < y.duration;
            case licenseColumn:  return SourceHelpers::getShortLicenseName (x.license) < SourceHelpers::getShortLicenseName (y.license);
            case formatColumn:   return x.format < y.format;
            case sizeColumn:     return x.filesize < y.filesize;
            default:             return false;
        }
    });
}

void SearchPanel::selectResult (int index, bool play)
{
    const auto* result = getResult (index);
    if (result == nullptr)
        return;
    selectedIndex = index;
    map.setSelectedIndex (index);
    const int row = indexToRow (index);
    if (row >= 0 && table.getSelectedRow() != row)
        table.selectRow (row, true, true);
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

void SearchPanel::replaceSoundWithResult (const juce::String& soundUUID, int index)
{
    const auto* result = getResult (index);
    if (result == nullptr)
        return;
    // Parameters of ACTION_REPLACE_SOUND_AUDIO: sound UUID, sound ID, name, user, license, preview URL, local file path, format, size, slices
    actions.send (ACTION_REPLACE_SOUND_AUDIO, { soundUUID, juce::String (result->id), SourceActionSender::sanitize (result->name),
                                                SourceActionSender::sanitize (result->username), result->license, result->previewURL, "",
                                                result->format, juce::String ((int) juce::jmin ((juce::int64) INT_MAX, result->filesize)), "" });
    statusLabel.setText ("Replaced a sound with \"" + result->name + "\"", juce::dontSendNotification);
    statusLabel.setColour (juce::Label::textColourId, SourceColours::accent);
    updateFromState (getPreset());
}

void SearchPanel::showReplaceMenu()
{
    // Menu with the sounds of the preset
    const auto preset = getPreset();
    juce::PopupMenu menu;
    menu.addSectionHeader ("Replace the audio of (settings are kept)");
    juce::StringArray uuids;
    for (const auto& sound : preset)
    {
        if (! sound.hasType (SourceIDs::SOUND) || (bool) sound.getProperty (SourceIDs::willBeDeleted, false))
            continue;
        uuids.add (sound.getProperty (SourceIDs::uuid).toString());
        const auto name = sound.getChildWithName (SourceIDs::SOUND_SAMPLE).getProperty (SourceIDs::name).toString();
        menu.addItem (uuids.size(), juce::String (uuids.size()) + ". " + (name.isNotEmpty() ? name : juce::String ("(no name)")));
    }
    if (uuids.isEmpty())
        menu.addItem (-1, "There are no sounds in the preset", false);
    const int index = selectedIndex;
    juce::Component::SafePointer<SearchPanel> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&replaceButton).withMaximumNumColumns (3),
                        [safe, uuids, index] (int choice)
                        {
                            if (safe != nullptr && choice > 0 && choice <= uuids.size())
                                safe->replaceSoundWithResult (uuids[choice - 1], index);
                        });
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
    table.updateContent();
    table.repaint();
    updateSelectedSoundInfo();
}

void SearchPanel::setPreviewState (int state)
{
    if (state == previewState)
        return;
    previewState = state;
    // Values of SourceSampler::PreviewState: idle, loading, playing, failed
    stopButton.setEnabled (state == 1 || state == 2);
    if (state == 1)
    {
        previewStatusLabel.setText ("Loading preview...", juce::dontSendNotification);
        previewStatusLabel.setColour (juce::Label::textColourId, SourceColours::accent);
    }
    else if (state == 3)
    {
        previewStatusLabel.setText ("The preview could not be downloaded", juce::dontSendNotification);
        previewStatusLabel.setColour (juce::Label::textColourId, SourceColours::warning);
    }
    else
    {
        previewStatusLabel.setText ({}, juce::dontSendNotification);
    }
}

void SearchPanel::updateSelectedSoundInfo()
{
    const auto* result = getResult (selectedIndex);
    for (auto* c : std::initializer_list<juce::Component*> { &playButton, &stopButton, &addButton, &replaceButton })
        c->setVisible (result != nullptr);
    if (result == nullptr)
    {
        selectedNameLabel.setText ({}, juce::dontSendNotification);
        selectedDetailsLabel.setText (results != nullptr && ! results->empty() ? "Click a sound to listen to it" : "", juce::dontSendNotification);
        return;
    }
    selectedNameLabel.setText (result->name, juce::dontSendNotification);
    const bool inPreset = soundIdsInPreset.count (result->id) > 0;
    juce::StringArray details { "by " + result->username, formatDuration (result->duration), SourceHelpers::getShortLicenseName (result->license),
                                result->format, formatSize (result->filesize), "Freesound #" + juce::String (result->id) };
    selectedDetailsLabel.setText (details.joinIntoString (" - ") + (inPreset ? "  (in preset)" : juce::String()), juce::dontSendNotification);
    addButton.setButtonText (inPreset ? "Add again" : "Add to preset");
}

//==============================================================================
int SearchPanel::getNumRows()
{
    return (int) rowOrder.size();
}

void SearchPanel::paintRowBackground (juce::Graphics& g, int row, int width, int height, bool selected)
{
    if (selected)
        g.fillAll (SourceColours::control.brighter (0.15f));
    else if (row % 2 == 1)
        g.fillAll (SourceColours::card.brighter (0.04f));
    const auto* result = getResult (rowToIndex (row));
    if (result != nullptr && soundIdsInPreset.count (result->id) > 0)
    {
        g.setColour (SourceColours::accent);
        g.fillRect (0, 0, 3, height);  // Marks sounds already in the preset
    }
    juce::ignoreUnused (width);
}

void SearchPanel::paintCell (juce::Graphics& g, int row, int columnId, int width, int height, bool)
{
    const auto* result = getResult (rowToIndex (row));
    if (result == nullptr)
        return;
    auto bounds = juce::Rectangle<int> (width, height).reduced (6, 0);
    if (columnId == colourColumn)
    {
        g.setColour (result->getColour());
        g.fillEllipse (juce::Rectangle<int> (width, height).toFloat().withSizeKeepingCentre (11.0f, 11.0f));
        return;
    }
    juce::String text;
    switch (columnId)
    {
        case nameColumn:     text = result->name; break;
        case userColumn:     text = result->username; break;
        case durationColumn: text = formatDuration (result->duration); break;
        case licenseColumn:  text = SourceHelpers::getShortLicenseName (result->license); break;
        case formatColumn:   text = result->format; break;
        case sizeColumn:     text = formatSize (result->filesize); break;
        default: break;
    }
    const bool isName = columnId == nameColumn;
    g.setColour (isName ? SourceColours::text : SourceColours::dimText);
    g.setFont (juce::Font (juce::FontOptions (isName ? 13.0f : 12.0f, isName ? juce::Font::bold : juce::Font::plain)));
    g.drawText (text, bounds, (columnId == durationColumn || columnId == sizeColumn) ? juce::Justification::centredRight
                                                                                     : juce::Justification::centredLeft, true);
}

juce::Component* SearchPanel::refreshComponentForCell (int row, int columnId, bool, juce::Component* existing)
{
    if (columnId != addColumn)
    {
        delete existing;
        return nullptr;
    }
    auto* cell = dynamic_cast<AddButtonCell*> (existing);
    if (cell == nullptr)
    {
        delete existing;
        cell = new AddButtonCell (*this);
    }
    const int index = rowToIndex (row);
    const auto* result = getResult (index);
    cell->update (index, result != nullptr && soundIdsInPreset.count (result->id) > 0);
    return cell;
}

void SearchPanel::cellClicked (int row, int, const juce::MouseEvent&)
{
    selectResult (rowToIndex (row), true);
}

void SearchPanel::sortOrderChanged (int columnId, bool forwards)
{
    sortColumn = columnId;
    sortForwards = forwards;
    updateRowOrder();
    table.updateContent();
    if (selectedIndex >= 0)
        table.selectRow (indexToRow (selectedIndex), true, true);
    table.repaint();
}

int SearchPanel::layoutAcousticFilters (juce::Rectangle<int> area, bool apply)
{
    // As many filters per row as fit
    const int gap = 18, height = AcousticFilter::squareSize + 6;
    const int perRow = juce::jmax (1, (area.getWidth() + gap) / (AcousticFilter::preferredWidth + gap));
    const int numRows = (acousticFilters.size() + perRow - 1) / perRow;
    if (apply)
        for (int i = 0; i < acousticFilters.size(); ++i)
            acousticFilters[i]->setBounds (area.getX() + (i % perRow) * (AcousticFilter::preferredWidth + gap),
                                           area.getY() + (i / perRow) * (height + 4), AcousticFilter::preferredWidth, height);
    return numRows * (height + 4) - 4;
}

//==============================================================================
void SearchPanel::resized()
{
    auto b = getLocalBounds();
    const int gap = 8;

    // Search controls: query on the first row, filters and view buttons on the second, acoustic properties below
    const int acousticHeight = layoutAcousticFilters ({ 0, 0, b.getWidth() - 20, 1000 }, false);
    searchCard.setBounds (b.removeFromTop (8 + rowHeight + 6 + rowHeight + 6 + acousticHeight + 8 + 26));
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
        c.removeFromTop (6);
        layoutAcousticFilters (c, true);
        // Status in the title row of the card
        statusLabel.setBounds (searchCard.getLocalBounds().reduced (10, 6).removeFromTop (20).withTrimmedLeft (160));
    }
    b.removeFromTop (gap);

    // Selected sound at the bottom
    selectedCard.setBounds (b.removeFromBottom (58));
    {
        auto c = selectedCard.getLocalBounds().reduced (10, 8);
        replaceButton.setBounds (c.removeFromRight (124).withSizeKeepingCentre (124, 28));
        c.removeFromRight (6);
        addButton.setBounds (c.removeFromRight (120).withSizeKeepingCentre (120, 28));
        c.removeFromRight (8);
        stopButton.setBounds (c.removeFromRight (28).withSizeKeepingCentre (28, 28));
        c.removeFromRight (4);
        playButton.setBounds (c.removeFromRight (28).withSizeKeepingCentre (28, 28));
        c.removeFromRight (10);
        auto top = c.removeFromTop (c.getHeight() / 2);
        previewStatusLabel.setBounds (top.removeFromRight (200));
        selectedNameLabel.setBounds (top);
        selectedDetailsLabel.setBounds (c);
    }
    b.removeFromBottom (gap);

    table.setBounds (b);
    map.setBounds (b);
}
