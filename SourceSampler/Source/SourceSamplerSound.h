/*
  ==============================================================================

    SourceSamplerSound.h
    Created: 23 Mar 2022 1:38:02pm
    Author:  Frederic Font Corbera

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>
#include "helpers_source.h"
#include "signalsmith-stretch.h"
#include "DownloadPool.h"
#include <array>


using Stretch = signalsmith::stretch::SignalsmithStretch<float>;


class SourceSound;


class SourceSamplerSound: public juce::SynthesiserSound, juce::Timer
{
public:
    //==============================================================================
    /** Creates a sampled sound from audio data that has already been loaded in memory.

        Must be called from the message thread (the sound binds to the given ValueTree state).

        @param _state               the SOUND_SAMPLE ValueTree for this sample
        @param _sourceSoundPointer  the SourceSound this sample belongs to
        @param audioData            the decoded audio (ownership is taken)
        @param _soundSampleRate     sample rate of the decoded audio
        @param _pluginSampleRate    sample rate of the host plugin
        @param _pluginBlockSize     block size of the host plugin
    */
    SourceSamplerSound (const juce::ValueTree& _state,
                        SourceSound* _sourceSoundPointer,
                        std::unique_ptr<juce::AudioBuffer<float>> audioData,
                        double _soundSampleRate,
                        double _pluginSampleRate,
                        int _pluginBlockSize);
    
    ~SourceSamplerSound() override;
    
    juce::ValueTree state;
    void bindState ();
    
    void timerCallback() override;
    
    SourceSound* getSourceSound() { return sourceSoundPointer; };
    
    /** Original (non-stretched) audio data. */
    const juce::AudioBuffer<float>* getAudioData() const noexcept { return data.get(); }

    /** Audio data used for playback (pitch shifted/time stretched version of the original data, or the original
        data itself if no stretching is applied). Callers must hold getPlaybackDataLock() while using it, because the
        stretch processing thread can swap the buffer. */
    const juce::AudioBuffer<float>& getPlaybackData() const noexcept { return *playbackData; }
    juce::SpinLock& getPlaybackDataLock() noexcept { return playbackDataLock; }

    /** Writes a low-resolution mono version of the audio to disk (used by the web UI to display waveforms). */
    static void writeWaveformFileToDisk (const juce::AudioBuffer<float>& buffer, double sampleRate, const juce::File& outputLocation);
    
    juce::String getUUID() { return state.getProperty(SourceIDs::uuid, "-"); };
    int getSoundId() { return soundId.get(); };
    juce::String getSoundName() { return name.get(); };

    //==============================================================================
    bool appliesToNote (int midiNoteNumber) override;
    bool appliesToChannel (int midiChannel) override;
    bool appliesToVelocity (int midiVelocity); // This method is not part of the base class as the base class does not support velocity layers
    int getCorrectedVelocity(int midiVelocity);
    float getCorrectedVelocity(float midiVelocity);
    
    //==============================================================================
    float getParameterFloat(juce::Identifier identifier);
    float gpf(juce::Identifier identifier) { return getParameterFloat(identifier);};
    int getParameterInt(juce::Identifier identifier);
    int gpi(juce::Identifier identifier) { return getParameterInt(identifier);};
    
    //==============================================================================
    void setSampleStartEndAndLoopPositions(float start, float end, float loopStart, float loopEnd);
    void checkSampleSampleStartEndAndLoopPositions();
    
    //==============================================================================
    int getLengthInSamples();
    float getLengthInSeconds();
    double getSoundSampleRate() const noexcept { return soundSampleRate; }
    
    //==============================================================================
    int getNumberOfMappedMidiNotes();
    juce::BigInteger getMappedMidiNotes();
    void setMappedMidiNotes(juce::BigInteger newMappedMidiNotes);
    int getMidiRootNote();
    void setMidiRootNote(int newRootNote);
    int getMidiVelocityLayer();
    void setMappedVelocities(juce::BigInteger newMappedVelocities);
    
    //==============================================================================
    /** Sets the manual slices (times in seconds of the original audio; message thread). */
    void setSliceTimes(const std::vector<float>& times);
    /** Positions (0..1 of the sound, sorted) of the manual slices. Audio thread: hold getPlaybackDataLock(). */
    const std::vector<float>& getSlicePositions() const noexcept { return slicePositions; }
    
    //==============================================================================
    bool isScheduledForDeletion();
    void scheduleSampleSoundDeletion();
    bool shouldBeDeleted();
    
    //==============================================================================
    void preProcessAudioWithStretch();
    void setStretchParameters(float newPitchShiftSemitones, float newTimeStretchRatio);
    bool isComputingStretch() const noexcept { return computingTimeStretch.load(); }
    
    class StretchProcessorThread : public juce::Thread
    {
    public:
        StretchProcessorThread(SourceSamplerSound& s) : juce::Thread ("StretchProcessorThread"), samplerSound (s){}
        
        void run() override
        {
            samplerSound.preProcessAudioWithStretch();
        }
        SourceSamplerSound& samplerSound;
    };
    StretchProcessorThread stretchProcessorThread;
    
private:
    //==============================================================================
    friend class SourceSamplerVoice;
    
    SourceSound* sourceSoundPointer = nullptr;
    
    // Parameters binded in state
    juce::CachedValue<juce::String> name;
    juce::CachedValue<int> soundId;
    juce::CachedValue<int> midiRootNote;
    juce::CachedValue<int> midiVelocityLayer;
    juce::CachedValue<float> sampleStartPosition;
    juce::CachedValue<float> sampleEndPosition;
    juce::CachedValue<float> sampleLoopStartPosition;
    juce::CachedValue<float> sampleLoopEndPosition;

    // "Volatile" properties that are not binded in state
    std::shared_ptr<const juce::AudioBuffer<float>> data;          // Original audio
    std::shared_ptr<const juce::AudioBuffer<float>> playbackData;  // Stretched audio (or same object as data if no stretching)
    juce::SpinLock playbackDataLock;
    std::atomic<int> lengthInSamples { 0 };  // Length of playbackData
    double soundSampleRate;
    double pluginSampleRate;
    int pluginBlockSize;
    std::vector<float> slicePositions = {};
    juce::BigInteger midiNotes = 0;
    juce::BigInteger midiVelocities = 0;
    
    // 3rd party time stretcher/pitch shifter
    static constexpr float maxTimeStretchRatio = 4.0f;
    Stretch stretch;  // Only used from the stretch processing thread
    std::atomic<float> nextTimeStretchRatio { 1.0f };
    std::atomic<float> nextPitchShiftSemitones { 0.0f };
    float appliedTimeStretchRatio = 1.0f;  // Only used from the message thread
    float appliedPitchShiftSemitones = 0.0f;  // Only used from the message thread
    double shouldProcessWithStretchAtTime = -1.0;
    std::atomic<bool> computingTimeStretch { false };
    
    // we use these properties to specify when a sample sound will be deleted. Unlike SourceSound, we don't bind this to state
    bool willBeDeleted = false;
    double scheduledForDeletionTime = 0.0;

    
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
    SourceSound (const juce::ValueTree& _state,
                 std::function<GlobalContextStruct()> globalContextGetter);
    ~SourceSound ();
    
    juce::ValueTree state;
    void bindState ();
    
    std::vector<SourceSamplerSound*> getLinkedSourceSamplerSounds();
    SourceSamplerSound* getFirstLinkedSourceSamplerSound();
    SourceSamplerSound* getLinkedSourceSamplerSoundWithUUID(const juce::String& sourceSamplerSoundUUID);
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
    
    /** Manual slices (times in seconds, used when numSlices is SLICE_MODE_MANUAL). */
    std::vector<float> getManualSlices() const;
    void setManualSlices(std::vector<float> times);  // Message thread (stored in the state)
    /** Onset times (seconds) of the first sample, from the Freesound analysis (empty if not available). */
    std::vector<float> getOnsetTimes() const;
    void setSlicesFromOnsets();
    
    // ------------------------------------------------------------------------------------------------
    
    void addOrEditMidiMapping(juce::String uuid, int ccNumber, juce::String parameterName, float minRange, float maxRange);
    std::vector<MidiCCMapping*> getMidiMappingsForCcNumber(int ccNumber);
    void removeMidiMapping(juce::String uuid);
    void applyMidiCCModulations(int channel, int number, int value);
    /** Last value (0..1) received for a CC number on this sound's channel, or -1 if none was received (message thread). */
    float getLastCcValue(int ccNumber) const { return ccNumber >= 0 && ccNumber < 128 ? lastCcValues[(size_t)ccNumber] : -1.0f; }
    
    // ------------------------------------------------------------------------------------------------
    // MPE mappings: map per-note MPE dimensions (slide, glide, strike) to sound parameters
    // (per-note pressure is handled by the aftertouch modulation parameters, mod2*). Unlike MIDI CC
    // mappings (which change the sound parameter for all notes), these are applied per voice in the audio thread.
    
    struct MpeMapping
    {
        SourceHelpers::MpeSource source = SourceHelpers::MpeSource::slide;
        juce::Identifier parameter;
        float minRange = 0.0f;
        float maxRange = 1.0f;
        float curve = 0.0f;
        float parameterMin = 0.0f;
        float parameterMax = 1.0f;
    };
    static constexpr int maxMpeMappings = 16;
    using MpeMappings = std::array<MpeMapping, maxMpeMappings>;
    
    void addOrEditMpeMapping(const juce::String& uuid, const juce::String& source, const juce::String& parameterName, float minRange, float maxRange, float curve);
    void removeMpeMapping(const juce::String& uuid);
    void updateMpeMappingsSnapshot();  // Message thread: rebuilds the copy of the mappings used by the audio thread
    /** Audio thread: copies the current mappings to dest. Returns the number of mappings, or -1 if the mappings are
        being updated (in that case dest is not modified and the caller should keep using its previous copy). */
    int copyMpeMappings(MpeMappings& dest) noexcept;
    
    // ------------------------------------------------------------------------------------------------
    // Modulators: internal signals (LFO, ADSR) that modulate a sound parameter independently for each voice (see
    // SourceHelpers::ModulatorType). Like MPE mappings, the audio thread uses a copy of the modulators.
    
    struct Modulator
    {
        SourceHelpers::ModulatorType type = SourceHelpers::ModulatorType::lfo;
        juce::Identifier parameter;
        float minRange = 0.0f;  // Normalised offsets (-1..1) added to the parameter
        float maxRange = 0.0f;
        int lfoWaveform = 0;
        float lfoRate = 1.0f;
        float attack = 0.5f, decay = 0.5f, sustain = 0.5f, release = 0.5f;
        float lfoAttack = 0.0f;  // Seconds (free LFOs)
        bool lfoSync = false;
        int lfoSyncDivision = 8;  // Index in SourceHelpers::getSyncDivisions (synced LFOs)
        int lfoAttackDivision = 0;  // Index in SourceHelpers::getSyncAttackNames (synced LFOs)
        int numSteps = 8;
        float stepsRate = 0.25f;  // Cycles per second (free steps)
        int stepsSyncLength = 4;  // Index in SourceHelpers::getStepsCycleLengths (synced steps)
        SourceHelpers::StepValues stepValues {};
        float parameterMin = 0.0f;
        float parameterMax = 1.0f;
    };
    static constexpr int maxModulators = 16;
    using Modulators = std::array<Modulator, maxModulators>;
    
    /** Adds a modulator, or edits the one with the given UUID. */
    void addOrEditModulator(const juce::String& uuid, const juce::String& type, const juce::String& parameterName, float minRange, float maxRange,
                            int lfoWaveform, float lfoRate, float attack, float decay, float sustain, float release,
                            float lfoAttack = SourceDefaults::lfoAttack, bool lfoSync = SourceDefaults::lfoSync,
                            int lfoSyncDivision = SourceDefaults::lfoSyncDivision, int lfoAttackDivision = SourceDefaults::lfoAttackDivision,
                            int numSteps = SourceDefaults::numSteps, float stepsRate = SourceDefaults::stepsRate,
                            int stepsSyncLength = SourceDefaults::stepsSyncLength, const juce::String& stepValues = {});
    void removeModulator(const juce::String& uuid);
    void updateModulatorsSnapshot();  // Message thread: rebuilds the copy of the modulators used by the audio thread
    /** Converts the parameters of older versions (mod2CutoffAmt, mod2GainAmt, mod2PitchAmt, mod2PlayheadPos,
        vel2CutoffAmt, vel2GainAmt) to aftertouch and velocity modulators (and removes them). */
    static void convertLegacyModulationParameters(juce::ValueTree& soundState);
    /** Audio thread: copies the current modulators to dest. Returns the number of modulators, or -1 if they are being
        updated (dest is then not modified and the caller should keep using its previous copy). */
    int copyModulators(Modulators& dest) noexcept;
    bool hasModulators() const noexcept { return numModulatorsAtomic.load (std::memory_order_relaxed) > 0; }
    /** Modulators, MPE mappings and MIDI CC mappings share this limit per sound (checked when adding new ones). */
    static constexpr int maxModulationsPerSound = 16;
    int getNumModulations() const;  // Number of modulators, MPE mappings and MIDI CC mappings
    
    /** Muted sounds keep playing (voices, modulators) but their output is not added to the mix. */
    void setMuted(bool shouldBeMuted);  // Message thread
    bool isMuted() const noexcept { return mutedFlag.load(std::memory_order_relaxed); }
    /** Parameters that can be modulated (the same as MIDI CC and MPE mapping targets). */
    static juce::StringArray getModulatableParameterNames();
    
    // ------------------------------------------------------------------------------------------------
    
    bool sourceSamplerSoundWithUUIDAlreadyCreated(const juce::String& sourceSamplerSoundUUID);
    void removeSourceSampleSoundsFromSampler();
    void removeSourceSamplerSound(const juce::String& samplerSoundUUID, int indexInSampler);
    void addNewSourceSamplerSoundFromValueTree(juce::ValueTree newSourceSamplerSound);
    void scheduleDeletionOfSourceSamplerSound(const juce::String& sourceSamplerSoundUUID);
    
    /** (Re)starts the loading process of the sound samples: downloads them if needed and, once all are available
        on disk, decodes them in a background thread and adds the resulting SourceSamplerSound(s) to the sampler.
        Must be called from the message thread. */
    void startLoading();
    /** Downloads again the files whose download failed, and loads the sound again (message thread). */
    void retryFailedDownloads();
    
    static bool isSupportedAudioFileFormat(const juce::String& extension);
    static bool fileLocationIsSupportedAudioFileFormat(juce::File location);
    static juce::File getFreesoundFileLocation(juce::ValueTree sourceSamplerSoundState, const GlobalContextStruct& context);
    static bool shouldUseOriginalQualityFile(juce::ValueTree sourceSamplerSoundState, const GlobalContextStruct& context);
    static bool fileAlreadyInDisk(juce::File locationInDisk);
    void downloadProgressUpdate(juce::File targetFileLocation, float percentageCompleted);
    void downloadFinished(juce::File targetFileLocation, bool taskSucceeded);
    
    juce::Synthesiser* getSampler() const noexcept { return sampler; }
    
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
    juce::CachedValue<float> velSensitivity;
    juce::CachedValue<int> midiChannel;
    juce::CachedValue<float> pitchShift;
    juce::CachedValue<float> timeStretch;
    // --> End auto-generated code A
    
    // Loading (see startLoading)
    struct LoadedSample
    {
        juce::String uuid;
        std::unique_ptr<juce::AudioBuffer<float>> buffer;
        double sampleRate = 0.0;
    };
    struct PendingDownload
    {
        juce::String url;
        juce::File location;
        juce::String extraHeaders;
    };
    void loadSounds(std::function<bool()> shouldStopLoading);  // Runs in the loader thread
    void onSamplesLoaded(std::vector<LoadedSample>& loadedSamples);  // Runs in the message thread
    void startDownloads(const std::vector<PendingDownload>& downloads);  // Runs in the message thread (never blocks)
    void postToMessageThread(std::function<void(SourceSound&)> callback);
    juce::ValueTree loaderStateSnapshot;  // Copy of the state used by the loader thread (so it never reads the live state)
    GlobalContextStruct loaderContext;  // Copy of the global context used by the loader thread
    juce::StringArray failedDownloads;  // Files whose download failed (not retried until the sound is loaded again)
    juce::StringArray loaderFailedDownloads;  // Copy of failedDownloads used by the loader thread
    
    // Other
    juce::Synthesiser* sampler = nullptr;
    juce::SharedResourcePointer<DownloadPool> downloadPool;
    DownloadPool::CancelFlag downloadsCancelFlag = DownloadPool::makeCancelFlag();
    double scheduledForDeletionTime = 0.0;
    juce::CriticalSection samplerSoundCreateDeleteLock;
    juce::CriticalSection midiMappingCreateDeleteLock;
    MpeMappings mpeMappings;
    int numMpeMappings = 0;
    juce::SpinLock mpeMappingsLock;
    std::array<float, 128> lastCcValues = makeNoCcValues();
    static std::array<float, 128> makeNoCcValues() { std::array<float, 128> values; values.fill(-1.0f); return values; }
    Modulators modulators;
    int numModulators = 0;
    std::atomic<int> numModulatorsAtomic { 0 };
    std::atomic<bool> mutedFlag { false };
    juce::SpinLock modulatorsLock;
    
    JUCE_DECLARE_WEAK_REFERENCEABLE (SourceSound)
    juce::WeakReference<SourceSound> weakThis;  // Created in the message thread so it can be safely copied from other threads
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
        return v.hasType (SourceIDs::SOUND);
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
        juce::ValueTree child = parent.getChildWithProperty(SourceIDs::uuid, uuid);
        if (child.isValid()){
            parent.removeChild(child, nullptr);
        }
    }
};
