/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "helpers.h"
#include "FreesoundAPI.h"
#include "ServerInterface.h"
#include "SourceSamplerSynthesiser.h"
#include "SourceSamplerSound.h"
#include "Downloader.h"
#include "LevelMeterSource.h"
#include "defines.h"


//==============================================================================
/**
*/
class SourceSamplerAudioProcessor  : public AudioProcessor,
                                     public ActionBroadcaster,
                                     public ActionListener,
                                     public Timer,
                                     protected juce::ValueTree::Listener
{
public:
    //==============================================================================
    SourceSamplerAudioProcessor();
    ~SourceSamplerAudioProcessor();
    
    juce::ValueTree state;
    void bindState();
    GlobalContextStruct getGlobalContext();
    
    //==============================================================================
    std::string exec(const char* cmd); // Util function to run in command line

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (AudioBuffer<float>&, MidiBuffer&) override;

    //==============================================================================
    AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const String getProgramName (int index) override;
    void changeProgramName (int index, const String& newName) override;
    String getPresetFilenameByIndex(int index);
    juce::CachedValue<int> currentPresetIndex;
    int latestLoadedPreset = 0; // Only used in ELK builds to re-load the last preset that was loaded (in previous runs included)

    //==============================================================================
    ValueTree collectPresetStateInformation ();
    void getStateInformation (MemoryBlock& destData) override;
    void loadPresetFromStateInformation (ValueTree state);
    void setStateInformation (const void* data, int sizeInBytes) override;
    void saveCurrentPresetToFile(const String& presetName, int index);
    bool loadPresetFromFile (const String& fileName);
    
    ValueTree collectGlobalSettingsStateInformation ();
    void saveGlobalPersistentStateToFile();
    void loadGlobalPersistentStateFromFile();
    
    ValueTree collectVolatileStateInformation ();
    String collectVolatileStateInformationAsString ();
    
    ValueTree collectFullStateInformation (bool skipVolatile);
    void sendStateToExternalServer(ValueTree state, String stringData);
    
    //==============================================================================
    void actionListenerCallback (const String &message) override;
    
    //==============================================================================
    File sourceDataLocation;
    File soundsDownloadLocation;
    File presetFilesLocation;
    File tmpFilesLocation;
    
    String useOriginalFilesPreference = USE_ORIGINAL_FILES_NEVER;
    
    File getPresetFilePath(const String& presetFilename);
    String getPresetFilenameFromNameAndIndex(const String& presetName, int index);
    File getGlobalSettingsFilePathFromName();
    
    //==============================================================================
    void setMidiInChannelFilter(int channel);
    void setMidiThru(bool doMidiTrhu);
    
    //==============================================================================
    void updateReverbParameters();
    
    //==============================================================================
    class QueryMakerThread : private Thread
    {
    public:
        QueryMakerThread(SourceSamplerAudioProcessor& p) : Thread ("SampleLoaderThread"), processor (p){}
        
        void setQueryParameters(const String& _query, int _numSounds, float _minSoundLength, float _maxSoundLength){
            query = _query;
            numSounds = _numSounds;
            minSoundLength = _minSoundLength;
            maxSoundLength = _maxSoundLength;
        }
        
        void run() override
        {
            processor.makeQueryAndLoadSounds(query, numSounds, minSoundLength, maxSoundLength);
        }
        SourceSamplerAudioProcessor& processor;
        String query;
        int numSounds;
        float minSoundLength;
        float maxSoundLength;
    };
    QueryMakerThread queryMakerThread;
    void makeQueryAndLoadSounds(const String& query, int numSounds, float minSoundLength, float maxSoundLength);
    bool isSupportedAudioFileFormat(const String& extension);
    bool fileLocationIsSupportedAudioFileFormat(File location);
    File getSoundPreviewLocation(ValueTree sound);
    File getSoundOriginalFileLocation(ValueTree sound);
    File getSoundLocalPathLocation(ValueTree sound);
    File getSoundFileLocationToLoad(ValueTree sound);
    void downloadSounds(bool blocking, int soundIndexFilter);
    bool allSoundsFinishedDownloading();
    
    class SampleLoaderThread : private Thread
    {
    public:
        SampleLoaderThread(SourceSamplerAudioProcessor& p) : Thread ("SampleLoaderThread"), processor (p){}
        
        void setSoundToLoad(int _soundIdx){
            soundIdx = _soundIdx;
        }
        
        void run() override
        {
            // NOTE: commented for testing purposes
            //processor.setSingleSourceSamplerSoundObject(soundIdx);
        }
        SourceSamplerAudioProcessor& processor;
        int soundIdx;
    };
    SampleLoaderThread sampleLoaderThread;
    void setSingleSourceSamplerSoundObject(int soundIdx);  // Create a sound object in the sampler corresponding to an element of "loadedSoundsInfo"
    void removeSound(int soundIdx); // Remove an element from "loadedSoundsInfo" and the corresponding sound in the sampler
    int addOrReplaceSoundFromSoundInfoValueTree(int soundIdx, ValueTree soundInfo);  // Add or replace an element of "loadedSoundsInfo" and trigger its download (and further replacement in the sampler)
    void addOrReplaceSoundFromBasicSoundProperties(int soundIdx,
                                                   int soundID,
                                                   const String& soundName,
                                                   const String& soundUser,
                                                   const String& soundLicense,
                                                   const String& oggDownloadURL,
                                                   const String& localFilePath,
                                                   const String& type,
                                                   int sizeBytes,
                                                   StringArray slices,
                                                   BigInteger midiNotes,
                                                   int midiRootNote,
                                                   const String& triggerDownloadSoundAction
                                                   );  // Replace an element of "loadedSoundsInfo" and trigger its download (and further replacement in the sampler)
    
    void reapplyNoteLayout(int newNoteLayoutType);
    
    void addToMidiBuffer(const juce::String& soundUUID, bool doNoteOff);

    double getStartTime();
    
    void timerCallback() override;
    int getServerInterfaceHttpPort();
    
    void previewFile(const String& path);
    void stopPreviewingFile();
    String currentlyLoadedPreviewFilePath = "";

    
protected:
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChildAdded (juce::ValueTree& parentTree, juce::ValueTree&) override;
    void valueTreeChildRemoved (juce::ValueTree& parentTree, juce::ValueTree&, int) override;
    void valueTreeChildOrderChanged (juce::ValueTree& parentTree, int, int) override;
    void valueTreeParentChanged (juce::ValueTree&) override;

    
private:
    AudioFormatManager audioFormatManager;
    SourceSamplerSynthesiser sampler;
    ServerInterface serverInterface;
    foleys::LevelMeterSource lms;  // Object to measure audio output levels
    Downloader downloader; // Object to download sounds in the background (or synchrounously)
    juce::OSCSender oscSender;  // Used to send state updates to glue app
    
    // Properties binded to state
    std::unique_ptr<SourceSoundList> sounds;
    juce::CachedValue<juce::String> presetName;
    juce::CachedValue<int> noteLayoutType;
    juce::CachedValue<float> reverbRoomSize;
    juce::CachedValue<float> reverbDamping;
    juce::CachedValue<float> reverbWetLevel;
    juce::CachedValue<float> reverbDryLevel;
    juce::CachedValue<float> reverbWidth;
    juce::CachedValue<float> reverbFreezeMode;
    
    // Other volatile properties
    bool oscSenderIsConnected = false;
    MidiBuffer midiFromEditor;
    bool midiOutForwardsMidiIn = true;
    int lastReceivedMIDIControllerNumber = -1;
    int lastReceivedMIDINoteNumber = -1;
    bool midiMessagesPresentInLastStateReport = false;
    bool isQueryDownloadingAndLoadingSounds = false;
    double startedQueryDownloadingAndLoadingSoundsTime = 0;
    double startTime;
    bool aconnectWasRun = false;
    ValueTree loadedSoundsInfo;
    
    // The next two objects are to preview sounds independently of the sampler
    std::unique_ptr<AudioFormatReaderSource> readerSource;
    AudioTransportSource transportSource;
    
    void logToState(const String& message);
    std::vector<String> recentLogMessages = {};
    String recentLogMessagesSerialized = "";

    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SourceSamplerAudioProcessor)
};
