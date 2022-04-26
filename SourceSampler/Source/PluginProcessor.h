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
#include "LevelMeterSource.h"
#include "defines.h"


//==============================================================================
/**
*/
class SourceSamplerAudioProcessor  : public juce::AudioProcessor,
                                     public juce::ActionBroadcaster,
                                     public juce::ActionListener,
                                     public juce::Timer,
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

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;
    juce::String getPresetFilenameByIndex(int index);
    juce::CachedValue<int> currentPresetIndex;
    int latestLoadedPreset = 0; // Only used in ELK builds to re-load the last preset that was loaded (in previous runs included)

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void loadPresetFromStateInformation (juce::ValueTree state);
    void setStateInformation (const void* data, int sizeInBytes) override;
    void saveCurrentPresetToFile(const juce::String& presetName, int index);
    bool loadPresetFromFile (const juce::String& fileName);
    
    void saveGlobalPersistentStateToFile();
    void loadGlobalPersistentStateFromFile();
    
    juce::ValueTree collectVolatileStateInformation ();
    juce::String collectVolatileStateInformationAsString ();
    void sendStateToExternalServer(juce::ValueTree state, juce::String stringData);
    
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
    void setMidiInChannelFilter(int channel);
    void setMidiThru(bool doMidiTrhu);
    
    //==============================================================================
    void updateReverbParameters();
    
    //==============================================================================
    class QueryMakerThread : public juce::Thread
    {
    public:
        QueryMakerThread(SourceSamplerAudioProcessor& p) : juce::Thread ("QueryMakerThread"), processor (p){}
        
        void setQueryParameters(const juce::String& _query, int _numSounds, float _minSoundLength, float _maxSoundLength){
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
        juce::String query;
        int numSounds;
        float minSoundLength;
        float maxSoundLength;
    };
    QueryMakerThread queryMakerThread;
    //==============================================================================
    void makeQueryAndLoadSounds(const juce::String& query, int numSounds, float minSoundLength, float maxSoundLength);
    void removeSound(const juce::String& soundUUID);
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
                                                   int midiRootNote);
    void addOrReplaceSoundFromBasicSoundProperties(const juce::String& soundUUID,
                                                   FSSound sound,
                                                   juce::BigInteger midiNotes,
                                                   int midiRootNote);
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
    
    std::unique_ptr<SourceSoundList> sounds;
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
    
    // The next two objects are to preview sounds independently of the sampler
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::AudioTransportSource transportSource;
    
    // Other
    juce::CriticalSection soundDeleteLock;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SourceSamplerAudioProcessor)
};
