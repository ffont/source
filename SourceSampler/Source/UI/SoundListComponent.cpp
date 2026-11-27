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
        }
        if ((bool) child.getProperty (SourceIDs::downloadFailed, false))
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
        for (auto* b : { &playButton, &stopButton, &removeButton })
            addAndMakeVisible (b);
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
        for (auto* b : { &playButton, &stopButton, &removeButton })
            b->setVisible (! isAllSoundsRow);
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
        content.removeFromRight (3 * 26 + 4);

        if (isAllSoundsRow)
        {
            g.setColour (SourceColours::text);
            g.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));
            g.drawText ("All sounds", content, juce::Justification::centredLeft);
            return;
        }

        // Activity indicator
        auto indicator = content.removeFromLeft (10).withSizeKeepingCentre (8, 8).toFloat();
        g.setColour (numActiveVoices > 0 ? SourceColours::accent : SourceColours::control);
        g.fillEllipse (indicator);
        content.removeFromLeft (6);

        auto topLine = content.removeFromTop (content.getHeight() / 2);
        g.setColour (item.willBeDeleted ? SourceColours::dimText : SourceColours::text);
        g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
        g.drawText (juce::String (row) + ". " + (item.name.isNotEmpty() ? item.name : "(no name)"), topLine, juce::Justification::centredLeft, true);

        g.setFont (juce::Font (juce::FontOptions (11.0f)));
        g.setColour (SourceColours::dimText);
        juce::String details;
        if (item.username.isNotEmpty())
            details << "by " << item.username << "  ";
        details << item.notesDescription;
        if (! item.loaded)
        {
            if (item.downloadProgress >= 100.0f)
                details = "loading...  " + details;
            else
                details = "downloading " + juce::String (juce::roundToInt (item.downloadProgress)) + "%  " + details;
        }
        if (item.numFailedDownloads > 0)
        {
            details = (item.numSamples > 1 ? juce::String (item.numFailedDownloads) + " sample(s) could not be downloaded  "
                                            : juce::String ("could not be downloaded  ")) + details;
            g.setColour (juce::Colour (0xffe07a7a));
        }
        g.drawText (details, content, juce::Justification::centredLeft, true);
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (6, 0);
        auto buttons = b.removeFromRight (3 * 26).withSizeKeepingCentre (3 * 26, 22);
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
