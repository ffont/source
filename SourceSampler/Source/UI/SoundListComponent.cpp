/*
  ==============================================================================

    SoundListComponent.cpp

  ==============================================================================
*/

#include "SoundListComponent.h"


static juce::String describeMappedNotes (const juce::BigInteger& notes, int& numMappedNotes)
{
    numMappedNotes = notes.countNumberOfSetBits();
    if (numMappedNotes == 0)
        return "no notes";
    auto noteName = [] (int n) { return juce::MidiMessage::getMidiNoteName (n, true, true, 3); };
    const int lowest = notes.findNextSetBit (0);
    const int highest = notes.getHighestBit();
    if (numMappedNotes == 1)
        return noteName (lowest);
    if (highest - lowest + 1 == numMappedNotes)
        return noteName (lowest) + " to " + noteName (highest);
    return juce::String (numMappedNotes) + " notes (" + noteName (lowest) + " to " + noteName (highest) + ")";
}


SoundListItem SoundListItem::fromState (const juce::ValueTree& soundState)
{
    SoundListItem item;
    item.uuid = soundState.getProperty (SourceIDs::uuid).toString();
    item.loaded = soundState.getProperty (SourceIDs::allSoundsLoaded, false);
    item.willBeDeleted = soundState.getProperty (SourceIDs::willBeDeleted, false);
    item.findingSimilar = soundState.getProperty (SourceIDs::findingSimilar, false);

    // Files are relative to the data location (stored in the root of the state)
    const juce::File dataLocation (soundState.getRoot().getProperty (SourceIDs::sourceDataLocation).toString());
    bool sizeKnown = true;
    juce::int64 sizeSum = 0;
    float progressSum = 0.0f;
    for (const auto& child : soundState)
    {
        if (! child.hasType (SourceIDs::SOUND_SAMPLE))
            continue;
        if (item.numSamples == 0)
        {
            item.name = child.getProperty (SourceIDs::name).toString();
            item.username = child.getProperty (SourceIDs::username).toString();
            item.license = child.getProperty (SourceIDs::license).toString();
            item.soundId = child.getProperty (SourceIDs::soundId, -1);
            item.rootNote = child.getProperty (SourceIDs::midiRootNote, -1);
            item.fromFreesound = (bool) child.getProperty (SourceIDs::soundFromFreesound, false) && item.soundId > 0;
        }
        // Size of what is downloaded: the file in disk if available, otherwise the original file size when the
        // original file will be downloaded (the size of previews is not known until they are downloaded)
        const auto filePath = child.getProperty (SourceIDs::filePath).toString();
        const auto file = (filePath.isNotEmpty() && dataLocation != juce::File()) ? dataLocation.getChildFile (filePath) : juce::File();
        if ((bool) child.getProperty (SourceIDs::downloadCompleted, false) && file.existsAsFile())
            sizeSum += file.getSize();
        else if (! (bool) child.getProperty (SourceIDs::usesPreview, true) && (int) child.getProperty (SourceIDs::filesize, -1) > 0)
            sizeSum += (int) child.getProperty (SourceIDs::filesize);
        else
            sizeKnown = false;

        if ((bool) child.getProperty (SourceIDs::localFileMissing, false))
        {
            item.numMissingLocalFiles += 1;
            progressSum += 100.0f;
        }
        else if ((bool) child.getProperty (SourceIDs::downloadFailed, false))
        {
            item.numFailedDownloads += 1;
            progressSum += 100.0f;
        }
        else
        {
            progressSum += (float) child.getProperty (SourceIDs::downloadProgress, 0.0f);
        }
        item.numSamples += 1;
    }
    item.downloadProgress = item.numSamples > 0 ? progressSum / (float) item.numSamples : 0.0f;
    item.downloaded = item.numSamples > 0 && item.downloadProgress >= 100.0f;
    item.downloadSizeBytes = (sizeKnown && item.numSamples > 0) ? sizeSum : -1;
    if (item.numSamples > 1)
        item.name << " (+" << (item.numSamples - 1) << " samples)";

    juce::BigInteger notes;
    notes.parseString (soundState.getProperty (SourceIDs::midiNotes, SourceDefaults::midiNotes).toString(), 16);
    item.notesDescription = describeMappedNotes (notes, item.numMappedNotes);
    return item;
}


//==============================================================================
class SoundListComponent::RowComponent : public juce::Component
{
public:
    explicit RowComponent (SoundListComponent& o) : owner (o)
    {
        for (auto* b : { &similarButton, &playButton, &stopButton, &removeButton })
            addAndMakeVisible (b);
        similarButton.setTooltip ("Replace with a similar sound from Freesound (keeping the settings)");
        similarButton.onClick = [this] { owner.actions.send (ACTION_REPLACE_SOUND_WITH_SIMILAR, { item.uuid }); };
        addChildComponent (retryButton);
        retryButton.setTooltip ("Retry downloading");
        retryButton.onClick = [this] { owner.actions.send (ACTION_RETRY_SOUND_DOWNLOADS, { item.uuid }); };
        playButton.onClick = [this] { owner.actions.send (ACTION_PLAY_SOUND, { item.uuid }); };
        stopButton.onClick = [this] { owner.actions.send (ACTION_STOP_SOUND, { item.uuid }); };
        removeButton.onClick = [this] { owner.actions.send (ACTION_REMOVE_SOUND, { item.uuid }); };
        setInterceptsMouseClicks (true, true);
    }

    void update (int newRow, bool selected)
    {
        row = newRow;
        isSelected = selected;
        isAllSoundsRow = (row == 0);
        if (! isAllSoundsRow && row - 1 < (int) owner.items.size())
            item = owner.items[(size_t) (row - 1)];
        else
            item = {};
        for (auto* b : { &similarButton, &playButton, &stopButton, &removeButton })
            b->setVisible (! isAllSoundsRow);
        similarButton.setEnabled (item.fromFreesound && ! item.findingSimilar && ! item.willBeDeleted);
        // Sounds can't be played until they are downloaded and loaded (or if some file could not be downloaded)
        const bool canPlay = item.loaded && item.numFailedDownloads == 0 && item.numMissingLocalFiles == 0;
        playButton.setEnabled (canPlay);
        stopButton.setEnabled (canPlay);
        const bool showRetry = ! isAllSoundsRow && item.numFailedDownloads > 0;
        if (retryButton.isVisible() != showRetry)
        {
            retryButton.setVisible (showRetry);
            resized();
        }
        auto it = owner.activeVoices.find (item.uuid);
        numActiveVoices = it != owner.activeVoices.end() ? it->second : 0;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().reduced (0, 1).toFloat();
        g.setColour (isSelected ? SourceColours::control.brighter (0.15f) : SourceColours::card);
        g.fillRoundedRectangle (bounds, 4.0f);

        // Download/loading progress, drawn as the background of the row: the filled part grows while the sound's files
        // are downloaded, then the whole row stays dimly filled while the audio is being decoded and loaded
        if (! isAllSoundsRow && ! item.loaded && ! item.willBeDeleted)
        {
            const float progress = juce::jlimit (0.0f, 1.0f, item.downloadProgress / 100.0f);
            const bool downloading = progress < 1.0f;
            juce::Graphics::ScopedSaveState saveState (g);
            juce::Path clip;
            clip.addRoundedRectangle (bounds, 4.0f);
            g.reduceClipRegion (clip);
            g.setColour (SourceColours::accent.withAlpha (downloading ? 0.28f : 0.16f));
            g.fillRect (bounds.withWidth (bounds.getWidth() * (downloading ? progress : 1.0f)));
        }
        if (isSelected)
        {
            g.setColour (SourceColours::accent);
            g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);
        }

        auto content = getLocalBounds().reduced (8, 4);
        content.removeFromRight ((retryButton.isVisible() ? 5 : 4) * 26 + 4);

        if (isAllSoundsRow)
        {
            g.setColour (SourceColours::text);
            g.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));
            g.drawText ("All sounds (" + juce::String ((int) owner.items.size()) + ")", content, juce::Justification::centredLeft);
            return;
        }

        // Activity indicator
        auto indicator = content.removeFromLeft (10).withSizeKeepingCentre (8, 8).toFloat();
        g.setColour (numActiveVoices > 0 ? SourceColours::accent : SourceColours::control);
        g.fillEllipse (indicator);
        content.removeFromLeft (6);

        auto topLine = content.removeFromTop (content.getHeight() / 2);
        // Title in orange while the sound has not been downloaded
        g.setColour (item.willBeDeleted ? SourceColours::dimText : (item.downloaded ? SourceColours::text : juce::Colour (0xfff0a040)));
        g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
        g.drawText (juce::String (row) + ". " + (item.name.isNotEmpty() ? item.name : "(no name)"), topLine, juce::Justification::centredLeft, true);

        g.setFont (juce::Font (juce::FontOptions (11.0f)));
        g.setColour (SourceColours::dimText);
        // Details: "by <user> - <notes> - <size>", followed by the download/loading status in parentheses
        juce::StringArray detailParts;
        if (item.username.isNotEmpty())
            detailParts.add ("by " + item.username);
        detailParts.add (item.notesDescription);
        if (item.downloadSizeBytes >= 0)
            detailParts.add (juce::String ((double) item.downloadSizeBytes / (1024.0 * 1024.0), 2) + " MB");
        juce::String status;
        auto statusColour = SourceColours::dimText;
        if (item.findingSimilar)
        {
            status = "(finding a similar sound...)";
            statusColour = SourceColours::accent;
        }
        else if (item.numMissingLocalFiles > 0)
        {
            // Local files can't be downloaded, so there is no retry button
            status = item.numSamples > 1 && item.numMissingLocalFiles < item.numSamples
                         ? "(" + juce::String (item.numMissingLocalFiles) + " of " + juce::String (item.numSamples) + " local files missing)"
                         : juce::String ("(no local file)");
            statusColour = juce::Colour (0xffe07a7a);
        }
        else if (item.numFailedDownloads > 0)
        {
            status = item.numSamples > 1 ? "(" + juce::String (item.numFailedDownloads) + " of " + juce::String (item.numSamples) + " samples failed downloading)"
                                         : juce::String ("(failed downloading)");
            statusColour = juce::Colour (0xffe07a7a);
        }
        else if (! item.loaded)
        {
            status = item.downloadProgress >= 100.0f ? juce::String ("(loading...)")
                                                     : "(downloading " + juce::String (juce::roundToInt (item.downloadProgress)) + "%)";
        }
        const juce::Font detailsFont (juce::FontOptions (11.0f));
        juce::AttributedString details;
        details.setWordWrap (juce::AttributedString::none);
        details.setJustification (juce::Justification::centredLeft);
        details.append (detailParts.joinIntoString (" - "), detailsFont, SourceColours::dimText);
        if (status.isNotEmpty())
            details.append ("  " + status, detailsFont, statusColour);
        juce::Graphics::ScopedSaveState clip (g);
        g.reduceClipRegion (content);
        details.draw (g, content.toFloat());
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (6, 0);
        auto buttons = b.removeFromRight (5 * 26).withSizeKeepingCentre (5 * 26, 22);
        if (retryButton.isVisible())
            retryButton.setBounds (buttons.removeFromLeft (24));
        buttons.removeFromLeft (2);
        buttons = buttons.removeFromRight (4 * 26 - 2);
        similarButton.setBounds (buttons.removeFromLeft (24));
        buttons.removeFromLeft (2);
        playButton.setBounds (buttons.removeFromLeft (24));
        buttons.removeFromLeft (2);
        stopButton.setBounds (buttons.removeFromLeft (24));
        buttons.removeFromLeft (2);
        removeButton.setBounds (buttons.removeFromLeft (24));
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        owner.listBox.selectRow (row);
    }

private:
    SoundListComponent& owner;
    int row = -1;
    bool isSelected = false;
    bool isAllSoundsRow = false;
    int numActiveVoices = 0;
    SoundListItem item;
    IconButton retryButton { "Retry", IconButton::Icon::retry };
    IconButton similarButton { "Similar", IconButton::Icon::similar };
    IconButton playButton { "Play", IconButton::Icon::play };
    IconButton stopButton { "Stop", IconButton::Icon::stop };
    IconButton removeButton { "Remove", IconButton::Icon::remove };
};


//==============================================================================
SoundListComponent::SoundListComponent (SourceActionSender actionSender) : actions (std::move (actionSender))
{
    listBox.setRowHeight (40);
    listBox.setColour (juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
    listBox.selectRow (0);
    addAndMakeVisible (listBox);

    clearAllButton.onClick = [this] { actions.send (ACTION_CLEAR_ALL_SOUNDS); };
    addAndMakeVisible (clearAllButton);

   #if JUCE_IOS
    emptyLabel.setText ("No sounds loaded. Search for sounds on Freesound above.", juce::dontSendNotification);
   #else
    emptyLabel.setText ("No sounds loaded. Search for sounds on Freesound above, or drop audio files here.", juce::dontSendNotification);
   #endif
    emptyLabel.setJustificationType (juce::Justification::centred);
    emptyLabel.setColour (juce::Label::textColourId, SourceColours::dimText);
    emptyLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (emptyLabel);
}

SoundListComponent::~SoundListComponent()
{
    listBox.setModel (nullptr);
}

void SoundListComponent::updateFromState (const juce::ValueTree& presetState)
{
    const juce::String previouslySelected = getSelectedSoundUUID();
    std::vector<SoundListItem> newItems;
    for (const auto& child : presetState)
        if (child.hasType (SourceIDs::SOUND))
            newItems.push_back (SoundListItem::fromState (child));

    const bool structureChanged = newItems.size() != items.size()
        || ! std::equal (newItems.begin(), newItems.end(), items.begin(),
                         [] (const SoundListItem& a, const SoundListItem& b) { return a.uuid == b.uuid; });
    items = std::move (newItems);
    emptyLabel.setVisible (items.empty());
    listBox.updateContent();

    if (structureChanged)
        selectSound (previouslySelected);  // Keep selection (falls back to "All sounds" if the sound is gone)
    listBox.repaint();
}

void SoundListComponent::setActiveVoices (const std::map<juce::String, int>& activeVoicesPerSound)
{
    if (activeVoicesPerSound == activeVoices)
        return;
    activeVoices = activeVoicesPerSound;
    listBox.updateContent();
    listBox.repaint();
}

juce::String SoundListComponent::getSelectedSoundUUID() const
{
    const int row = listBox.getSelectedRow();
    if (row >= 1 && row - 1 < (int) items.size())
        return items[(size_t) (row - 1)].uuid;
    return {};
}

void SoundListComponent::selectSound (const juce::String& uuid)
{
    int row = 0;
    for (size_t i = 0; i < items.size(); ++i)
        if (items[i].uuid == uuid && uuid.isNotEmpty())
            row = (int) i + 1;
    if (listBox.getSelectedRow() != row)
        listBox.selectRow (row);
}

juce::Component* SoundListComponent::refreshComponentForRow (int rowNumber, bool isRowSelected, juce::Component* existing)
{
    if (rowNumber >= getNumRows())
    {
        delete existing;
        return nullptr;
    }
    auto* row = dynamic_cast<RowComponent*> (existing);
    if (row == nullptr)
    {
        delete existing;
        row = new RowComponent (*this);
    }
    row->update (rowNumber, isRowSelected);
    return row;
}

void SoundListComponent::selectedRowsChanged (int)
{
    if (onSelectionChanged != nullptr)
        onSelectionChanged (getSelectedSoundUUID());
}

void SoundListComponent::paint (juce::Graphics&) {}

void SoundListComponent::resized()
{
    auto b = getLocalBounds();
    clearAllButton.setBounds (b.removeFromBottom (24).removeFromLeft (130));
    b.removeFromBottom (6);
    listBox.setBounds (b);
    emptyLabel.setBounds (b.withTrimmedTop (listBox.getRowHeight()).reduced (10));
}
