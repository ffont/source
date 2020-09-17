/*
  ==============================================================================

    This file was auto-generated!

    It contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "FreesoundAPI.h"
#include "ServerInterface.h"
#include "SourceSampler.h"
#include "defines.h"


//==============================================================================
/**
*/
class SourceSamplerAudioProcessor  : public AudioProcessor,
                                     public ActionBroadcaster,
                                     public ActionListener
{
public:
    //==============================================================================
    SourceSamplerAudioProcessor();
    ~SourceSamplerAudioProcessor();
    
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
    int currentPresetIndex = 0;

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
    void updatePresetNumberMapping(const String& presetName, int index);
    void sendStateToServer(ValueTree state);
    
    ValueTree collectVolatileStateInformation ();
    
    //==============================================================================
    // Action listener
    void actionListenerCallback (const String &message) override;
    
    //==============================================================================
    File sourceDataLocation;
    File soundsDownloadLocation;
    File presetFilesLocation;
    
    File getPresetFilePath(const String& presetFilename);
    String getPresetFilenameFromNameAndIndex(const String& presetName, int index);
    File getGlobalSettingsFilePathFromName();
    
    //==============================================================================
    void setMidiInChannelFilter(int channel);
    void setMidiThru(bool doMidiTrhu);
    
    //==============================================================================
    void makeQueryAndLoadSounds(const String& query, int numSounds, float maxSoundLength);
    void downloadSoundsAndSetSources(ValueTree soundsInfo);
    void setSources(int midiNoteRootOffset);
    
    void addToMidiBuffer(int soundNumber, bool doNoteOff);

    double getStartTime();
    String getQuery();
    ValueTree getLoadedSoundsInfo();
    
private:
    SourceSamplerSynthesiser sampler;
    AudioFormatManager audioFormatManager;
    
    MidiBuffer midiFromEditor;
    long midicounter;
    std::vector<std::unique_ptr<URL::DownloadTask>> downloadTasks;
    bool isQueryinAndDownloadingSounds = false;
    double startTime;
    bool aconnectWasRun = false;
    
    String query = "";
    String presetName = "empty";
    ValueTree loadedSoundsInfo;
    bool midiOutForwardsMidiIn = true;
    
    ValueTree presetNumberMapping = ValueTree(GLOBAL_PERSISTENT_STATE_PRESETS_MAPPING);
    
    ServerInterface serverInterface;
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SourceSamplerAudioProcessor)
};
