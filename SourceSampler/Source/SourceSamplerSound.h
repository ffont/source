/*
  ==============================================================================

    SourceSamplerSound.h
    Created: 23 Mar 2022 1:38:02pm
    Author:  Frederic Font Corbera

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>
#include "defines.h"
#include "helpers.h"


class SourceSamplerSound: public SynthesiserSound
{
public:
    //==============================================================================
    /** Creates a sampled sound from an audio reader.

        This will attempt to load the audio from the source into memory and store
        it in this object.

        @param name         a name for the sample
        @param source       the audio to load. This object can be safely deleted by the
                            caller after this constructor returns
        @param maxSampleLengthSeconds   a maximum length of audio to read from the audio
                                        source, in seconds
        @param _pluginSampleRate  sample rate of the host plugin
        @param _pluginBlockSize  block size of the host plugin
    */
    SourceSamplerSound (int _idx,
                        const String& name,
                        AudioFormatReader& source,
                        bool _loadingPreviewVersion,
                        double maxSampleLengthSeconds,
                        double _pluginSampleRate,
                        int _pluginBlockSize);
    
    SourceSamplerSound (const juce::ValueTree& state);

    /** Destructor. */
    ~SourceSamplerSound() override;
   
    //==============================================================================
    /** Returns the sample's name */
    const String& getName() const noexcept                  { return name; }

    /** Returns the audio sample data.
        This could return nullptr if there was a problem loading the data.
    */
    AudioBuffer<float>* getAudioData() const noexcept       { return data.get(); }
    
    bool getLoadedPreviewVersion();

    //==============================================================================
    bool appliesToNote (int midiNoteNumber) override;
    bool appliesToChannel (int midiChannel) override;
    
    //==============================================================================
    void setParameterByNameFloat(const String& name, float value);
    void setParameterByNameFloatNorm(const String& name, float value0to1);
    void setParameterByNameInt(const String& name, int value);
    
    //==============================================================================
    ValueTree getState();
    void loadState(ValueTree soundState);
    
    //==============================================================================
    int getLengthInSamples();
    float getLengthInSeconds();
    float getPlayingPositionPercentage();
    
    void setIdx(int newIdx);
    int getIdx();
    
    //==============================================================================
    int getNumberOfMappedMidiNotes();
    void setMappedMidiNotes(BigInteger newMappedMidiNotes);
    BigInteger getMappedMidiNotes();
    int getMidiRootNote();
    
    void setOnsetTimesSamples(std::vector<float> _onsetTimes);
    std::vector<int> getOnsetTimesSamples();
    
    void addOrEditMidiMapping(int randomId, int ccNumber, String parameterName, float minRange, float maxRange);
    std::vector<MidiCCMapping> getMidiMappingsForCcNumber(int ccNumber);
    std::vector<MidiCCMapping> getMidiMappingsSorted();
    void removeMidiMapping(int randomID);

private:
    //==============================================================================
    friend class SourceSamplerVoice;
    
    int idx = -1;  // This is the idx of the sound in the loadedSoundsInfo ValueTree stored in the plugin processor

    juce::String name;
    bool loadedPreviewVersion = false;
    std::unique_ptr<AudioBuffer<float>> data;
    double soundSampleRate;
    BigInteger midiNotes;
    int length = 0;
    std::vector<int> onsetTimesSamples = {};
    std::vector<MidiCCMapping> midiMappings = {};
    
    double pluginSampleRate;
    int pluginBlockSize;

    // Define sound "controllable" parameters here
    
    // --> Start auto-generated code A
    int launchMode = 0;
    float startPosition = 0.0f;
    float endPosition = 1.0f;
    float loopStartPosition = 0.0f;
    float loopEndPosition = 1.0f;
    int loopXFadeNSamples = 500;
    int reverse = 0;
    int noteMappingMode = 0;
    int numSlices = 0;
    float playheadPosition = 0.0f;
    float freezePlayheadSpeed = 100.0f;
    float filterCutoff = 20000.0f;
    float filterRessonance = 0.0f;
    float filterKeyboardTracking = 0.0f;
    ADSR::Parameters filterADSR = {0.01f, 0.0f, 1.0f, 0.01f};
    float filterADSR2CutoffAmt = 1.0f;
    float gain = -10.0f;
    ADSR::Parameters ampADSR = {0.01f, 0.0f, 1.0f, 0.01f};
    float pan = 0.0f;
    int midiRootNote = 64;
    float pitch = 0.0f;
    float pitchBendRangeUp = 12.0f;
    float pitchBendRangeDown = 12.0f;
    float mod2CutoffAmt = 10.0f;
    float mod2GainAmt = 6.0f;
    float mod2PitchAmt = 0.0f;
    float mod2PlayheadPos = 0.0f;
    float vel2CutoffAmt = 0.0f;
    float vel2GainAmt = 0.5f;
    // --> End auto-generated code A

    JUCE_LEAK_DETECTOR (SourceSamplerSound)
};


//==============================================================================
// The classes below deal with Sound objects as represented in the state value
// instead of sound objects that will indeed load sounds. When needed, these objects
// can generate the actual SourceSamplerSound objects that will be loaded in the
// sampler. Note that a single SourceSound object can result in the creation of
// several SourceSamplerSound objects, eg in the case of multi-layered sounds
// or sounds sampled at different pitches.


class SourceSound
{
public:
    SourceSound (const juce::ValueTree& _state): state(_state)
    {
        bindState();
    }
    
    ~SourceSound(){}
    
    juce::ValueTree state;
    
    void bindState()
    {
        name.referTo(state, IDs::name, nullptr);
        enabled.referTo(state, IDs::enabled, nullptr);
    }
    
    void setName(const juce::String& newName) {
        name = newName;
    }
    
private:
    juce::CachedValue<juce::String> name;
    juce::CachedValue<bool> enabled;
    
    JUCE_LEAK_DETECTOR (SourceSound)
};


struct SourceSoundList: public drow::ValueTreeObjectList<SourceSound>
{
    SourceSoundList (const juce::ValueTree& v)
    : drow::ValueTreeObjectList<SourceSound> (v)
    {
        rebuildObjects();
    }

    ~SourceSoundList()
    {
        freeObjects();
    }

    bool isSuitableType (const juce::ValueTree& v) const override
    {
        return v.hasType (IDs::SOUND);
    }

    SourceSound* createNewObject (const juce::ValueTree& v) override
    {
        return new SourceSound (v);
    }

    void deleteObject (SourceSound* c) override
    {
        delete c;
    }

    void newObjectAdded (SourceSound*) override    {}
    void objectRemoved (SourceSound*) override     {}
    void objectOrderChanged() override       {}

};
