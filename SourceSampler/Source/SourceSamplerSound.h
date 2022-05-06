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


class SourceSound;


class SourceSamplerSound: public juce::SynthesiserSound
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
    SourceSamplerSound (const juce::ValueTree& _state,
                        SourceSound* _sourceSoundPointer,
                        juce::AudioFormatReader& source,
                        double maxSampleLengthSeconds,
                        double _pluginSampleRate,
                        int _pluginBlockSize);
    
    ~SourceSamplerSound() override;
    
    juce::ValueTree state;
    void bindState ();
    
    SourceSound* getSourceSound() { return sourceSoundPointer; };
    
    juce::AudioBuffer<float>* getAudioData() const noexcept { return data.get(); }
    void writeBufferToDisk();
    
    juce::String getUUID() { return state.getProperty(IDs::uuid, "-"); };
    int getSoundId() { return soundId.get(); };
    juce::String getSoundName() { return name.get(); };

    //==============================================================================
    bool appliesToNote (int midiNoteNumber) override;
    bool appliesToChannel (int midiChannel) override;
    bool appliesToVelocity (int midiVelocity); // This method is not part of the base class as the base class does not support velocity layers
    
    //==============================================================================
    float getParameterFloat(juce::Identifier identifier);
    float gpf(juce::Identifier identifier) { return getParameterFloat(identifier);};
    int getParameterInt(juce::Identifier identifier);
    float gpi(juce::Identifier identifier) { return getParameterInt(identifier);};
    
    //==============================================================================
    int getLengthInSamples();
    float getLengthInSeconds();
    float getPlayingPositionPercentage();
    
    //==============================================================================
    int getNumberOfMappedMidiNotes();
    juce::BigInteger getMappedMidiNotes();
    void setMappedMidiNotes(juce::BigInteger newMappedMidiNotes);
    int getMidiRootNote();
    void setMidiRootNote(int newRootNote);
    int getMidiVelocityLayer();
    void setMappedVelocities(juce::BigInteger newMappedVelocities);
    
    //==============================================================================
    void loadOnsetTimesSamplesFromAnalysis();
    void setOnsetTimesSamples(std::vector<float> _onsetTimes);
    std::vector<int> getOnsetTimesSamples();
    
private:
    //==============================================================================
    friend class SourceSamplerVoice;
    
    SourceSound* sourceSoundPointer = nullptr;
    
    // Parameters binded in state
    juce::CachedValue<juce::String> name;
    juce::CachedValue<int> soundId;
    juce::CachedValue<int> midiRootNote;
    juce::CachedValue<int> midiVelocityLayer;

    // "Volatile" properties that are not binded in state
    std::unique_ptr<juce::AudioBuffer<float>> data;
    int lengthInSamples = 0;
    double soundSampleRate;
    double pluginSampleRate;
    int pluginBlockSize;
    std::vector<int> onsetTimesSamples = {};
    juce::BigInteger midiNotes = 0;
    juce::BigInteger midiVelocities = 0;

    
    JUCE_LEAK_DETECTOR (SourceSamplerSound)
};


//==============================================================================
// The classes below deal with Sound objects as represented in the state value
// instead of sound objects that will indeed load sounds. When needed, these objects
// can generate the actual SourceSamplerSound objects that will be loaded in the
// sampler. Note that a single SourceSound object can result in the creation of
// several SourceSamplerSound objects, eg in the case of multi-layered sounds
// or sounds sampled at different pitches.


class SourceSound: public juce::URL::DownloadTask::Listener
{
public:
    SourceSound (const juce::ValueTree& _state,
                 std::function<GlobalContextStruct()> globalContextGetter);
    ~SourceSound ();
    
    juce::ValueTree state;
    void bindState ();
    
    std::vector<SourceSamplerSound*> getLinkedSourceSamplerSounds();
    SourceSamplerSound* getFirstLinkedSourceSamplerSound();
    juce::String getUUID();
    bool isScheduledForDeletion();
    void scheduleSoundDeletion();
    bool shouldBeDeleted();
    
    std::function<GlobalContextStruct()> getGlobalContext;
    
    // --------------------------------------------------------------------------------------------
    
    int getParameterInt(juce::Identifier identifier);
    float getParameterFloat(juce::Identifier identifier, bool normed);
    void setParameterByNameFloat(juce::Identifier identifier, float value, bool normed);
    void setParameterByNameInt(juce::Identifier identifier, int value);
    
    // ------------------------------------------------------------------------------------------------
    
    juce::BigInteger getMappedMidiNotes();
    int getNumberOfMappedMidiNotes();
    void setMappedMidiNotes(juce::BigInteger newMappedMidiNotes);
    void assignMidiNotesAndVelocityToSourceSamplerSounds();
    void setMidiRootNote(int newMidiRootNote);
    int getMidiNoteFromFirstSourceSamplerSound();
    
    // ------------------------------------------------------------------------------------------------
    
    void setOnsetTimesSamples(std::vector<float> onsetTimes);
    
    // ------------------------------------------------------------------------------------------------
    
    void addOrEditMidiMapping(juce::String uuid, int ccNumber, juce::String parameterName, float minRange, float maxRange);
    std::vector<MidiCCMapping*> getMidiMappingsForCcNumber(int ccNumber);
    void removeMidiMapping(juce::String uuid);
    
    // ------------------------------------------------------------------------------------------------
    
    std::vector<SourceSamplerSound*> createSourceSamplerSounds();
    void addSourceSamplerSoundsToSampler();
    void removeSourceSampleSoundsFromSampler();
    
    void loadSounds(std::function<bool()> shouldStopLoading);
    bool isSupportedAudioFileFormat(const juce::String& extension);
    bool fileLocationIsSupportedAudioFileFormat(juce::File location);
    juce::File getFreesoundFileLocation(juce::ValueTree sourceSamplerSoundState);
    bool shouldUseOriginalQualityFile(juce::ValueTree sourceSamplerSoundState);
    bool fileAlreadyInDisk(juce::File locationInDisk);
    void downloadProgressUpdate(juce::File targetFileLocation, float percentageCompleted);
    void downloadFinished(juce::File targetFileLocation, bool taskSucceeded);
    
    void progress (juce::URL::DownloadTask *task, juce::int64 bytesDownloaded, juce::int64 totalLength);
    void finished(juce::URL::DownloadTask *task, bool success);
    
    //==============================================================================
    class SoundLoaderThread : public juce::Thread
    {
    public:
        SoundLoaderThread(SourceSound& s) : juce::Thread ("SoundLoaderThread"), sound (s){}
        
        void run() override
        {
            sound.loadSounds([this]{return threadShouldExit();});
        }
        SourceSound& sound;
    };
    SoundLoaderThread soundLoaderThread;
    //==============================================================================
    
private:
    // Sound properties
    juce::CachedValue<bool> willBeDeleted;
    juce::CachedValue<bool> allSoundsLoaded;
    
    juce::CachedValue<juce::String> midiNotesAsString;
    std::unique_ptr<MidiCCMappingList> midiCCmappings;

    // --> Start auto-generated code A
    juce::CachedValue<int> launchMode;
    juce::CachedValue<float> startPosition;
    juce::CachedValue<float> endPosition;
    juce::CachedValue<float> loopStartPosition;
    juce::CachedValue<float> loopEndPosition;
    juce::CachedValue<int> loopXFadeNSamples;
    juce::CachedValue<int> reverse;
    juce::CachedValue<int> noteMappingMode;
    juce::CachedValue<int> numSlices;
    juce::CachedValue<float> playheadPosition;
    juce::CachedValue<float> freezePlayheadSpeed;
    juce::CachedValue<float> filterCutoff;
    juce::CachedValue<float> filterRessonance;
    juce::CachedValue<float> filterKeyboardTracking;
    juce::CachedValue<float> filterAttack;
    juce::CachedValue<float> filterDecay;
    juce::CachedValue<float> filterSustain;
    juce::CachedValue<float> filterRelease;
    juce::CachedValue<float> filterADSR2CutoffAmt;
    juce::CachedValue<float> gain;
    juce::CachedValue<float> attack;
    juce::CachedValue<float> decay;
    juce::CachedValue<float> sustain;
    juce::CachedValue<float> release;
    juce::CachedValue<float> pan;
    juce::CachedValue<float> pitch;
    juce::CachedValue<float> pitchBendRangeUp;
    juce::CachedValue<float> pitchBendRangeDown;
    juce::CachedValue<float> mod2CutoffAmt;
    juce::CachedValue<float> mod2GainAmt;
    juce::CachedValue<float> mod2PitchAmt;
    juce::CachedValue<float> mod2PlayheadPos;
    juce::CachedValue<float> vel2CutoffAmt;
    juce::CachedValue<float> vel2GainAmt;
    juce::CachedValue<int> midiChannel;
    // --> End auto-generated code A
    
    // Other
    std::vector<std::unique_ptr<juce::URL::DownloadTask>> downloadTasks;
    bool allDownloaded = false;
    double scheduledForDeletionTime = 0.0;
    std::function<bool()> shouldStopLoading;
    
    JUCE_LEAK_DETECTOR (SourceSound)
};


struct SourceSoundList: public drow::ValueTreeObjectList<SourceSound>
{
    SourceSoundList (const juce::ValueTree& v,
                     std::function<GlobalContextStruct()> globalContextGetter)
    : drow::ValueTreeObjectList<SourceSound> (v)
    {
        getGlobalContext = globalContextGetter;
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
        return new SourceSound (v, getGlobalContext);
    }

    void deleteObject (SourceSound* s) override
    {
        // Before deleting the object, the SourceSamplerSounds need to be deleted as well as these
        // point to the object itself. Here we might need to apply some tricks to make this RT safe.
        s->removeSourceSampleSoundsFromSampler();
        delete s;
    }

    void newObjectAdded (SourceSound* s) override    {}
    void objectRemoved (SourceSound*) override     {}
    void objectOrderChanged() override       {}
    
    std::function<GlobalContextStruct()> getGlobalContext;
    
    SourceSound* getSoundAt(int position) {
        if ((position >= 0) && (position < objects.size())){
            return objects[position];
        }
        return nullptr;
    }
    
    SourceSound* getSoundWithUUID(const juce::String& uuid) {
        for (auto* sound: objects){
            if (sound->getUUID() == uuid){
                return sound;
            }
        }
        return nullptr;
    }
    
    int getIndexOfSoundWithUUID(const juce::String& uuid) {
        int index = 0;
        for (auto* sound: objects){
            if (sound->getUUID() == uuid){
                return index;
            }
            index += 1;
        }
        return -1;
    }
    
    void removeSoundWithUUID(const juce::String& uuid){
        juce::ValueTree child = parent.getChildWithProperty(IDs::uuid, uuid);
        if (child.isValid()){
            parent.removeChild(child, nullptr);
        }
    }
};
