/*
  ==============================================================================

    SourceSampler.h
    Created: 13 Jan 2023 1:41:00pm
    Author:  Frederic Font Corbera

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "helpers_source.h"
#include "FreesoundAPI.h"
#include "ServerInterface.h"
#include "SourceSamplerSynthesiser.h"
#include "SourceSamplerSound.h"
#include "LevelMeterSource.h"


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
    SourceSampler();
    ~SourceSampler();
    
    juce::ValueTree state;
    void bindState();
    GlobalContextStruct getGlobalContext();
    
    void createDirectories(const juce::String& appDirectoryName);
    
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
    void makeQueryAndLoadSounds(const juce::String& addReplaceOrReplaceSound, const juce::String& query, int numSounds, float minSoundLength, float maxSoundLength);
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
                                                   bool addToExistingSourceSampleSounds);
    void addOrReplaceSoundFromBasicSoundProperties(const juce::String& soundUUID,
                                                   FSSound sound,  // Uses FSSound object from Freesound API
                                                   juce::BigInteger midiNotes,
                                                   int midiRootNote,
                                                   int midiVelocityLayer,
                                                   bool addToExistingSourceSampleSounds);
    void reapplyNoteLayout(int newNoteLayoutType);
    void addToMidiBuffer(const juce::String& soundUUID, bool doNoteOff);
    double getStartTime();
    
    void timerCallback() override;
    int getServerInterfaceHttpPort();
    int getServerInterfaceWSPort();
    
    void previewFile(const juce::String& path);
    void stopPreviewingFile();
    juce::String currentlyLoadedPreviewFilePath = "";
    
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
    foleys::LevelMeterSource lms;  // Object to measure audio output levels
    
    // Properties binded to state
    juce::CachedValue<int> globalMidiInChannel;
    juce::CachedValue<int> midiOutForwardsMidiIn;
    juce::CachedValue<juce::String> useOriginalFilesPreference;
    juce::CachedValue<juce::String> freesoundOauthAccessToken;
    
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
    bool isQuerying = false;
    juce::MidiBuffer midiFromEditor;
    int lastReceivedMIDIControllerNumber = -1;
    int lastReceivedMIDINoteNumber = -1;
    bool midiMessagesPresentInLastStateReport = false;
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
    
    // Other
    juce::CriticalSection soundDeleteLock;
    bool loadedPresetAtElkStartup = false;
    double sampleRate = 44100.0;
    int blockSize = 512;
    int numberOfChannels = 2;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SourceSampler)
};
