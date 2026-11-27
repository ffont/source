/*
  ==============================================================================

    SourceSampler.h
    Created: 13 Jan 2023 1:41:00pm
    Author:  Frederic Font Corbera

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <array>
#include "helpers_source.h"
#include "FreesoundAPI.h"
#include "ServerInterface.h"
#include "SourceSamplerSynthesiser.h"
#include "SourceSamplerSound.h"


//==============================================================================
/**
*/
class SourceSampler: public juce::ActionBroadcaster,
                     public juce::ActionListener,
                     public juce::Timer,
                     protected juce::ValueTree::Listener
{
public:
    //==============================================================================
    /** @param baseDataLocation  folder where the "SourceSampler" data folder (sounds, presets, settings) is created.
                                  If not specified, the user's documents folder is used (or the platform-specific
                                  location for ELK/iOS builds). */
    explicit SourceSampler (juce::File baseDataLocation = {});
    ~SourceSampler() override;
    
    /** The whole sampler state. Only modify it (and only listen to it) from the message thread. */
    juce::ValueTree state;
    void bindState();
    GlobalContextStruct getGlobalContext();
    
    void createDirectories(const juce::String& appDirectoryName, juce::File baseLocation);
    
    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock);
    void releaseResources();
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&);
    double getSampleRate();
    int getBlockSize();
    int getTotalNumOutputChannels();
    void setTotalNumOutputChannels(int numChannels);

    //==============================================================================
    juce::String getPresetFilenameByIndex(int index);
    juce::String getPresetNameByIndex(int index);
    void renamePreset(int index, const juce::String& newName);
    juce::CachedValue<int> currentPresetIndex;
    int latestLoadedPreset = 0; // Only used in ELK builds to re-load the last preset that was loaded (in previous runs included)

    void loadPresetFromStateInformation (juce::ValueTree state);
    void saveCurrentPresetToFile(const juce::String& presetName, int index);
    bool loadPresetFromFile (const juce::String& fileName);
    void loadPresetFromIndex(int index);
    
    void saveGlobalPersistentStateToFile();
    void loadGlobalPersistentStateFromFile();
    
    juce::ValueTree collectVolatileStateInformation ();
    juce::String collectVolatileStateInformationAsString ();
    
    //==============================================================================
    void actionListenerCallback (const juce::String &message) override;
    
    //==============================================================================
    juce::File sourceDataLocation;
    juce::File soundsDownloadLocation;
    juce::File presetFilesLocation;
    juce::File tmpFilesLocation;
    
    juce::File getPresetFilePath(const juce::String& presetFilename);
    juce::String getPresetFilenameFromNameAndIndex(const juce::String& presetName, int index);
    juce::File getGlobalSettingsFilePathFromName();
    
    //==============================================================================
    void setGlobalMidiInChannel(int channel);
    void setMidiThru(bool doMidiTrhu);
    void setMpeSettings(bool enabled, int perNotePitchBendRange);
    
    //==============================================================================
    void updateReverbParameters();
    
    //==============================================================================
    class QueryMakerThread : public juce::Thread
    {
    public:
        QueryMakerThread(SourceSampler& s) : juce::Thread ("QueryMakerThread"), sampler (s){}
        
        void setQueryParameters(const juce::String& _addReplaceOrReplaceSound, const juce::String& _query, int _numSounds, float _minSoundLength, float _maxSoundLength){
            addReplaceOrReplaceSound = _addReplaceOrReplaceSound;
            query = _query;
            numSounds = _numSounds;
            minSoundLength = _minSoundLength;
            maxSoundLength = _maxSoundLength;
        }
        
        void run() override
        {
            sampler.makeQueryAndLoadSounds(addReplaceOrReplaceSound, query, numSounds, minSoundLength, maxSoundLength);
        }
        SourceSampler& sampler;
        juce::String addReplaceOrReplaceSound;
        juce::String query;
        int numSounds;
        float minSoundLength;
        float maxSoundLength;
    };
    QueryMakerThread queryMakerThread;
    //==============================================================================
    void makeQueryAndLoadSounds(const juce::String& addReplaceOrReplaceSound, const juce::String& query, int numSounds, float minSoundLength, float maxSoundLength);  // Runs in QueryMakerThread
    void loadQueryResults(const juce::String& addReplaceOrReplaceSound, int numSounds, int numResults, juce::Array<FSSound> soundsFound);  // Runs in the message thread
    void removeSound(const juce::String& soundUUID);
    void removeSamplerSound(const juce::String& soundUUID, const juce::String& samplerSoundUUID);
    void removeAllSounds();
    void addOrReplaceSoundFromBasicSoundProperties(const juce::String& soundUUID,
                                                   int soundID,
                                                   const juce::String& soundName,
                                                   const juce::String& soundUser,
                                                   const juce::String& soundLicense,
                                                   const juce::String& oggDownloadURL,
                                                   const juce::String& localFilePath,
                                                   const juce::String& format,
                                                   int sizeBytes,
                                                   juce::StringArray slices,
                                                   juce::BigInteger midiNotes,
                                                   int midiRootNote,
                                                   int midiVelocityLayer,
                                                   int midiChannel,
                                                   bool addToExistingSourceSampleSounds);
    void addOrReplaceSoundFromBasicSoundProperties(const juce::String& soundUUID,
                                                   FSSound sound,  // Uses FSSound object from Freesound API
                                                   juce::BigInteger midiNotes,
                                                   int midiRootNote,
                                                   int midiVelocityLayer,
                                                   int midiChannel,
                                                   bool addToExistingSourceSampleSounds);
    void reapplyNoteLayout(int newNoteLayoutType);
    SourceSound* getSoundWithUUID(const juce::String& soundUUID) { return sounds != nullptr ? sounds->getSoundWithUUID(soundUUID) : nullptr; }
    void addToMidiBuffer(const juce::String& soundUUID, bool doNoteOff);
    double getStartTime();
    
    void timerCallback() override;
    int getServerInterfaceHttpPort();
    int getServerInterfaceWSPort();
    
    void previewFile(const juce::String& path);
    void stopPreviewingFile();
    juce::String currentlyLoadedPreviewFilePath = "";
    
    //==============================================================================
    // Information for UIs which is not part of the state ("volatile" state). Call from the message thread.
    struct VoiceInfo
    {
        bool active = false;
        juce::String soundUUID;  // UUID of the SourceSound being played
        juce::String firstSampleUUID;  // UUID of the first SOUND_SAMPLE of that sound (used by the web/Python UIs)
        float playPosition = -1.0f;  // Relative position (0-1) in the sound being played
    };
    std::vector<VoiceInfo> getVoicesInfo();
    std::vector<float> getOutputLevels();
    bool isQueryingSounds() const noexcept { return isQuerying.load(); }
    int getLastReceivedMidiNote() const noexcept { return lastReceivedMIDINoteNumber.load(); }
    int getLastReceivedMidiCC() const noexcept { return lastReceivedMIDIControllerNumber.load(); }
    
    /** Session autosave (used by the iOS standalone app, where the OS can kill the app at any time without giving
        it a chance to save). When enabled, the last saved session is restored (asynchronously) and the state is
        saved to the given file a couple of seconds after it changes. */
    void enableSessionAutosave (const juce::File& file);
    juce::File getSessionAutosaveFile() const { return sessionAutosaveFile; }
    void saveSessionNow();
    
    /** Preset import folder (used by the iOS standalone app, where the presets folder is in the app group container
        shared with the AUv3, which is not visible in the Files app). Preset files ("<index>.xml") copied to the import
        folder (which is visible in the Files app) are moved to the presets folder, replacing presets with the same
        index. The folder is checked now and then periodically. Does nothing if the folder is the presets folder. */
    void enablePresetImportFolder (const juce::File& folder);
    /** Rewrites old Freesound preview URLs in the preset files (done when the sampler is created). Returns the
        number of files changed. */
    int fixFreesoundPreviewURLsInPresetFiles();
    juce::File getPresetImportFolder() const { return presetImportFolder; }
    /** Moves valid preset files from the import folder to the presets folder. Returns the number of imported presets. */
    int importPresetsFromImportFolder();
    
    /** Runs the function in the message thread: synchronously if already in the message thread, asynchronously otherwise. */
    void runOnMessageThread (std::function<void(SourceSampler&)> fn);
    
protected:
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChildAdded (juce::ValueTree& parentTree, juce::ValueTree&) override;
    void valueTreeChildRemoved (juce::ValueTree& parentTree, juce::ValueTree&, int) override;
    void valueTreeChildOrderChanged (juce::ValueTree& parentTree, int, int) override;
    void valueTreeParentChanged (juce::ValueTree&) override;

    
private:
    juce::AudioFormatManager audioFormatManager;
    SourceSamplerSynthesiser sampler;
    ServerInterface serverInterface;
    // Output levels: RMS per channel averaged over ~200ms, computed in the audio thread and read by UIs (lock free)
    static constexpr int maxMeteredChannels = 8;
    std::array<std::atomic<float>, maxMeteredChannels> outputRmsLevels {};
    std::array<float, maxMeteredChannels> outputMeanSquares {};  // Audio thread only
    float levelSmoothingCoefficient = 0.0f;  // Per-block smoothing factor (set in prepareToPlay)
    
    // Properties binded to state
    juce::CachedValue<int> globalMidiInChannel;
    juce::CachedValue<int> midiOutForwardsMidiIn;
    juce::CachedValue<juce::String> useOriginalFilesPreference;
    juce::CachedValue<juce::String> freesoundOauthAccessToken;
    juce::CachedValue<bool> mpeEnabled;
    juce::CachedValue<int> mpePitchBendRange;
    
    std::unique_ptr<SourceSoundList> sounds;
    std::unique_ptr<SourceSoundList> soundsOld;  // Used when sounds is replaced to not immediately delete all objects inside and give time for save deletion
    juce::CachedValue<juce::String> presetName;
    juce::CachedValue<int> numVoices;
    juce::CachedValue<int> noteLayoutType;
    juce::CachedValue<float> reverbRoomSize;
    juce::CachedValue<float> reverbDamping;
    juce::CachedValue<float> reverbWetLevel;
    juce::CachedValue<float> reverbDryLevel;
    juce::CachedValue<float> reverbWidth;
    juce::CachedValue<float> reverbFreezeMode;
    
    // Other "volatile" properties
    std::atomic<bool> isQuerying { false };
    juce::MidiBuffer midiFromEditor;  // Notes triggered from UIs, protected by midiFromEditorLock
    juce::SpinLock midiFromEditorLock;
    void addMidiFromEditor (const juce::MidiMessage& message);
    std::atomic<int> lastReceivedMIDIControllerNumber { -1 };
    std::atomic<int> lastReceivedMIDINoteNumber { -1 };
    std::atomic<bool> midiMessagesPresentInLastStateReport { false };
    double startTime;
    double lastTimeIsAliveWasSent = 0;
    bool aconnectWasRun = false;
    
    // State sync stuff
    bool oscSenderIsConnected = false;
    int stateUpdateID = 0;
    juce::OSCSender oscSender;  // Used to send state updates to glue app
    void sendOSCMessage(const juce::OSCMessage& message);
    void sendWSMessage(const juce::OSCMessage& message);
    
    // The next two objects are to preview sounds independently of the sampler
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::AudioTransportSource transportSource;
    
    // Session autosave
    juce::File sessionAutosaveFile;
    bool sessionNeedsSaving = false;
    juce::File presetImportFolder;
    double lastPresetImportCheckTime = 0.0;
    double lastStateChangeTime = 0.0;
    
    // MIDI CC mappings and program changes received in the audio thread are applied here (message thread)
    void processPendingControlEvents();
    juce::TimedCallback controlEventsTimer { [this] { processPendingControlEvents(); } };
    
    // Other
    juce::CriticalSection soundDeleteLock;
    bool loadedPresetAtElkStartup = false;
    double sampleRate = 44100.0;
    int blockSize = 512;
    int numberOfChannels = 2;

    //==============================================================================
    JUCE_DECLARE_WEAK_REFERENCEABLE (SourceSampler)
    juce::WeakReference<SourceSampler> weakThis;  // Created in the message thread so it can be safely copied from other threads
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SourceSampler)
};
