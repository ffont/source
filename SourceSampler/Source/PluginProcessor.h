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

    //==============================================================================
    void getStateInformation (MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    
    //==============================================================================
    // Action listener
    void actionListenerCallback (const String &message) override;
    
    //==============================================================================
    void makeQueryAndLoadSounds(const String& query, int numSounds, float maxSoundLength);
    File soundsDownloadLocation;
    void newSoundsReadyToDownload(Array<FSSound> sounds, String textQuery, std::vector<juce::StringArray> soundInfo);

    void setSources(int midiNoteRootOffset);
    void addToMidiBuffer(int soundNumber);

    double getStartTime();
    bool isArrayNotEmpty();
    String getQuery();
    std::vector<juce::StringArray> getData();
    
private:
    
    bool isQueryinAndDownloadingSounds = false;
    std::vector<std::unique_ptr<URL::DownloadTask>> downloadTasks;
    SourceSamplerSynthesiser sampler;
    AudioFormatManager audioFormatManager;
    MidiBuffer midiFromEditor;
    long midicounter;
    double startTime;
    String query;
    std::vector<juce::StringArray> loadedSoundsInfo;
    
    ServerInterface serverInterface;
    
    bool aconnectWasRun = false;
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SourceSamplerAudioProcessor)
};
