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
    
    
    SourceSound* sourceSoundPointer = nullptr;
    void setSourceSoundPointer(SourceSound* _sourceSoundPointer)
    {
        sourceSoundPointer = _sourceSoundPointer;
    }
   
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

    
    float getParameterFloat(const String& name);
    int getParameterInt(const String& name);
    
    
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


class SourceSound: public juce::URL::DownloadTask::Listener,
                   public juce::Thread
{
public:
    SourceSound (const juce::ValueTree& _state,
                 std::function<GlobalContextStruct()> globalContextGetter): state(_state), Thread ("LoaderThread")
    {
        getGlobalContext = globalContextGetter;
        bindState();
        
        triggerSoundDownloads();
    }
    
    ~SourceSound ()
    {
        for (int i = 0; i < downloadTasks.size(); i++) {
            downloadTasks.at(i).reset();
        }
    }
    
    juce::ValueTree state;
    
    void bindState ()
    {
        name.referTo(state, IDs::name, nullptr);
        enabled.referTo(state, IDs::enabled, nullptr);
        launchMode.referTo(state, IDs::launchMode, nullptr);
        startPosition.referTo(state, IDs::startPosition, nullptr);
        endPosition.referTo(state, IDs::endPosition, nullptr);
        pitch.referTo(state, IDs::pitch, nullptr);
    }
    
    std::vector<SourceSamplerSound*> createSourceSamplerSounds ()
    {
        // Generate all the SourceSamplerSound objects corresponding to this sound
        // In most of the cases this will be a single sound, but it could be that
        // some sounds have more than one SourceSamplerSound (multi-layered sounds
        // for example)
        
        AudioFormatManager audioFormatManager;
        audioFormatManager.registerBasicFormats();
        std::vector<SourceSamplerSound*> sounds = {};
        
        for (int i=0; i<state.getNumChildren(); i++){
            auto child = state.getChild(i);
            if (child.hasType(IDs::SOUND_SAMPLE)){
                int soundId = (int)child.getProperty(IDs::soundId, -1);
                if (soundId > -1){
                    juce::String soundName = name + "-" + (juce::String)soundId;
                    File audioSample = File::getSpecialLocation(File::userDocumentsDirectory)
                        .getChildFile("SourceSampler/")
                        .getChildFile("sounds")
                        .getChildFile((juce::String)soundId).withFileExtension("ogg");
                    std::unique_ptr<AudioFormatReader> reader(audioFormatManager.createReaderFor(audioSample));
                    SourceSamplerSound* createdSound = new SourceSamplerSound(soundId,
                                                                              soundName,
                                                                              *reader,
                                                                              true,
                                                                              MAX_SAMPLE_LENGTH,
                                                                              getGlobalContext().sampleRate,
                                                                              getGlobalContext().samplesPerBlock);
                    createdSound->setSourceSoundPointer(this);
                    BigInteger midiNotes;
                    midiNotes.parseString(child.getProperty(IDs::midiNotes).toString(), 16);
                    createdSound->setMappedMidiNotes(midiNotes);
                    sounds.push_back(createdSound);
                }
            }
        }
        return sounds;
    }
    
    void addSourceSamplerSoundsToSampler()
    {
        // TODO: add lock here?
        std::vector<SourceSamplerSound*> sourceSamplerSounds = createSourceSamplerSounds();
        for (auto sourceSamplerSound: sourceSamplerSounds) {
            SourceSamplerSound* justAddedSound = static_cast<SourceSamplerSound*>(getGlobalContext().sampler->addSound(sourceSamplerSound));
        }
        std::cout << "Added " << sourceSamplerSounds.size() << " SourceSamplerSound(s) to sampler... " << std::endl;
    }
    
    void removeSourceSampleSoundsFromSampler()
    {
        // TODO: add lock here?
        
        std::vector<int> soundIndexesToDelete;
        for (int i=0; i<getGlobalContext().sampler->getNumSounds(); i++){
            auto* sourceSamplerSound = static_cast<SourceSamplerSound*>(getGlobalContext().sampler->getSound(i).get());
            if (sourceSamplerSound->sourceSoundPointer == this){
                // If the pointer to sourceSound of the sourceSamplerSound is the current sourceSound, then the sound should be deleted
                soundIndexesToDelete.push_back(i);
            }
        }
        
        int numDeleted = 0;
        for (auto idx: soundIndexesToDelete)
        {
            getGlobalContext().sampler->removeSound(idx - numDeleted);  // Compensate index updates as sounds get removed
            numDeleted += 1;
        }
        std::cout << "Removed " << numDeleted << " SourceSamplerSound(s) from sampler... " << std::endl;
    }
    
    int getParameterInt(const String& name){
        if (name == "launchMode") {
            return launchMode;
        }
        return -1;
    }
    
    float getParameterFloat(const String& name){
        if (name == "startPosition") { return startPosition; }
        else if (name == "endPosition") { return endPosition; }
        else if (name == "pitch") { return pitch; }
        return -1.0;
    }
    
    void setParameterByNameFloat(const String& name, float value){
        if (name == "startPosition") { startPosition = jlimit(0.0f, 1.0f, value); }
        else if (name == "endPosition") { endPosition = jlimit(0.0f, 1.0f, value); }
        else if (name == "pitch") { pitch = jlimit(-36.0f, 36.0f, value); }
        
        // Do some checking of start/end loop start/end positions to make sure we don't do anything wrong
        if (endPosition < startPosition) {
            endPosition = startPosition.get();
        }
        /*
        if (loopStartPosition < startPosition){
            loopStartPosition = startPosition;
        }
        if (loopEndPosition > endPosition){
            loopEndPosition = endPosition;
        }
        if (loopStartPosition > loopEndPosition){
            loopStartPosition = loopEndPosition;
        }*/
    }
    
    void setParameterByNameInt(const String& name, int value){
        if (name == "launchMode") { launchMode = jlimit(0, 4, value); }
    }
    
    void run(){
    }
    
    void triggerSoundDownloads()
    {
        bool allAlreadyDownloaded = true;
        for (int i=0; i<state.getNumChildren(); i++){
            auto child = state.getChild(i);
            if (child.hasType(IDs::SOUND_SAMPLE)){
                int soundId = (int)child.getProperty(IDs::soundId, -1);
                juce::String previewURL = child.getProperty(IDs::previewURL, "").toString();
                if ((previewURL != "") && (soundId > -1)){
                    juce::File locationInDisk = getGlobalContext().soundsDownloadLocation
                        .getChildFile((juce::String)soundId).withFileExtension("ogg");
                    child.setProperty(IDs::filePath, locationInDisk.getFullPathName(), nullptr);
                    if (!locationInDisk.exists()){
                        // Trigger download if sound not in disk
                        allAlreadyDownloaded = false;
                        child.setProperty(IDs::downloadProgress, 0.0, nullptr);
                        child.setProperty(IDs::downloadCompleted, false, nullptr);
                        URL::DownloadTaskOptions options = URL::DownloadTaskOptions().withListener(this);
                        std::unique_ptr<juce::URL::DownloadTask> downloadTask = juce::URL(previewURL).downloadToFile(locationInDisk, options);
                        downloadTasks.push_back(std::move(downloadTask));
                        std::cout << "Downloading sound to " << locationInDisk.getFullPathName() << std::endl;
                    } else {
                        // If sound already downloaded, save info in VT
                        child.setProperty(IDs::downloadProgress, 100.0, nullptr);
                        child.setProperty(IDs::downloadCompleted, true, nullptr);
                    }
                }
            }
        }
        
        if (allAlreadyDownloaded){
            // If no sound needs downloading
            addSourceSamplerSoundsToSampler();
        }
    }
    
    void finished(URL::DownloadTask *task, bool success){
        // TODO: load sound into buffer so it is already loaded before calling addSourceSamplerSoundsToSampler (and can be done in a background thread?)
        bool allAlreadyDownloaded = true;
        for (int i=0; i<state.getNumChildren(); i++){
            auto child = state.getChild(i);
            if (child.hasType(IDs::SOUND_SAMPLE)){
                File locationInDisk = File(child.getProperty(IDs::filePath, "").toString());
                if (task->getTargetLocation() == locationInDisk){
                    // Find the sample that corresponds to this download task and update state
                    child.setProperty(IDs::downloadCompleted, true, nullptr);
                    child.setProperty(IDs::downloadProgress, 100.0, nullptr);
                } else {
                    if (!child.getProperty(IDs::downloadCompleted, false)){
                        allAlreadyDownloaded = false;
                    }
                }
            }
        }
        
        if (allAlreadyDownloaded){
            // If all sounds finished downloading
            addSourceSamplerSoundsToSampler();
        }
    }
    
    void progress (URL::DownloadTask *task, int64 bytesDownloaded, int64 totalLength){
        for (int i=0; i<state.getNumChildren(); i++){
            auto child = state.getChild(i);
            if (child.hasType(IDs::SOUND_SAMPLE)){
                File locationInDisk = File(child.getProperty(IDs::filePath, "").toString());
                if (task->getTargetLocation() == locationInDisk){
                    // Find the sample that corresponds to this download task and update state
                    child.setProperty(IDs::downloadProgress, 100.0*(float)bytesDownloaded/(float)totalLength, nullptr);
                }
            }
        }
    }
    
private:
    // Sound properties
    juce::CachedValue<juce::String> name;
    juce::CachedValue<bool> enabled;
    juce::CachedValue<int> launchMode;
    juce::CachedValue<float> startPosition;
    juce::CachedValue<float> endPosition;
    juce::CachedValue<float> pitch;
    
    // Sound downloading
    std::vector<std::unique_ptr<URL::DownloadTask>> downloadTasks;
    bool allDownloaded = false;
    
    // Other
    std::function<GlobalContextStruct()> getGlobalContext;
    
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
};
