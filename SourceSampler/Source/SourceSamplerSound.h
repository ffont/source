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
    void setSourceSoundPointer(SourceSound* _sourceSoundPointer) { sourceSoundPointer = _sourceSoundPointer; }
   
    AudioBuffer<float>* getAudioData() const noexcept { return data.get(); }
    
    bool getLoadedPreviewVersion();

    //==============================================================================
    bool appliesToNote (int midiNoteNumber) override;
    bool appliesToChannel (int midiChannel) override;
    
    //==============================================================================
    void setParameterByNameFloat(const String& name, float value);
    void setParameterByNameFloatNorm(const String& name, float value0to1);
    void setParameterByNameInt(const String& name, int value);
    float getParameterFloat(const String& name);
    float gpf(const String& name) { return getParameterFloat(name);};
    int getParameterInt(const String& name);
    float gpi(const String& name) { return getParameterInt(name);};
    
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
                 std::function<GlobalContextStruct()> globalContextGetter): Thread ("LoaderThread"), state(_state)
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
        
        // --> Start auto-generated code C
        launchMode.referTo(state, IDs::launchMode, nullptr, Defaults::launchMode);
        startPosition.referTo(state, IDs::startPosition, nullptr, Defaults::startPosition);
        endPosition.referTo(state, IDs::endPosition, nullptr, Defaults::endPosition);
        loopStartPosition.referTo(state, IDs::loopStartPosition, nullptr, Defaults::loopStartPosition);
        loopEndPosition.referTo(state, IDs::loopEndPosition, nullptr, Defaults::loopEndPosition);
        loopXFadeNSamples.referTo(state, IDs::loopXFadeNSamples, nullptr, Defaults::loopXFadeNSamples);
        reverse.referTo(state, IDs::reverse, nullptr, Defaults::reverse);
        noteMappingMode.referTo(state, IDs::noteMappingMode, nullptr, Defaults::noteMappingMode);
        numSlices.referTo(state, IDs::numSlices, nullptr, Defaults::numSlices);
        playheadPosition.referTo(state, IDs::playheadPosition, nullptr, Defaults::playheadPosition);
        freezePlayheadSpeed.referTo(state, IDs::freezePlayheadSpeed, nullptr, Defaults::freezePlayheadSpeed);
        filterCutoff.referTo(state, IDs::filterCutoff, nullptr, Defaults::filterCutoff);
        filterRessonance.referTo(state, IDs::filterRessonance, nullptr, Defaults::filterRessonance);
        filterKeyboardTracking.referTo(state, IDs::filterKeyboardTracking, nullptr, Defaults::filterKeyboardTracking);
        filterA.referTo(state, IDs::filterA, nullptr, Defaults::filterA);
        filterD.referTo(state, IDs::filterD, nullptr, Defaults::filterD);
        filterS.referTo(state, IDs::filterS, nullptr, Defaults::filterS);
        filterR.referTo(state, IDs::filterR, nullptr, Defaults::filterR);
        filterADSR2CutoffAmt.referTo(state, IDs::filterADSR2CutoffAmt, nullptr, Defaults::filterADSR2CutoffAmt);
        gain.referTo(state, IDs::gain, nullptr, Defaults::gain);
        ampA.referTo(state, IDs::ampA, nullptr, Defaults::ampA);
        ampD.referTo(state, IDs::ampD, nullptr, Defaults::ampD);
        ampS.referTo(state, IDs::ampS, nullptr, Defaults::ampS);
        ampR.referTo(state, IDs::ampR, nullptr, Defaults::ampR);
        pan.referTo(state, IDs::pan, nullptr, Defaults::pan);
        midiRootNote.referTo(state, IDs::midiRootNote, nullptr, Defaults::midiRootNote);
        pitch.referTo(state, IDs::pitch, nullptr, Defaults::pitch);
        pitchBendRangeUp.referTo(state, IDs::pitchBendRangeUp, nullptr, Defaults::pitchBendRangeUp);
        pitchBendRangeDown.referTo(state, IDs::pitchBendRangeDown, nullptr, Defaults::pitchBendRangeDown);
        mod2CutoffAmt.referTo(state, IDs::mod2CutoffAmt, nullptr, Defaults::mod2CutoffAmt);
        mod2GainAmt.referTo(state, IDs::mod2GainAmt, nullptr, Defaults::mod2GainAmt);
        mod2PitchAmt.referTo(state, IDs::mod2PitchAmt, nullptr, Defaults::mod2PitchAmt);
        mod2PlayheadPos.referTo(state, IDs::mod2PlayheadPos, nullptr, Defaults::mod2PlayheadPos);
        vel2CutoffAmt.referTo(state, IDs::vel2CutoffAmt, nullptr, Defaults::vel2CutoffAmt);
        vel2GainAmt.referTo(state, IDs::vel2GainAmt, nullptr, Defaults::vel2GainAmt);
        // --> End auto-generated code C
    }
    
    const juce::String& getName() {
        return name.get();
    }
    
    bool isEnabled() {
        return enabled.get();
    }
    
    int getParameterInt(const juce::String& name){
        // --> Start auto-generated code E
        if (name == "launchMode") { return launchMode.get(); }
        else if (name == "loopXFadeNSamples") { return loopXFadeNSamples.get(); }
        else if (name == "reverse") { return reverse.get(); }
        else if (name == "noteMappingMode") { return noteMappingMode.get(); }
        else if (name == "numSlices") { return numSlices.get(); }
        else if (name == "midiRootNote") { return midiRootNote.get(); }
        // --> End auto-generated code E
        throw std::runtime_error("No int parameter with this name");
    }
    
    float getParameterFloat(const juce::String& name, bool normed){
        // --> Start auto-generated code F
        if (name == "startPosition") { return !normed ? startPosition.get() : jmap(startPosition.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (name == "endPosition") { return !normed ? endPosition.get() : jmap(endPosition.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (name == "loopStartPosition") { return !normed ? loopStartPosition.get() : jmap(loopStartPosition.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (name == "loopEndPosition") { return !normed ? loopEndPosition.get() : jmap(loopEndPosition.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (name == "playheadPosition") { return !normed ? playheadPosition.get() : jmap(playheadPosition.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (name == "freezePlayheadSpeed") { return !normed ? freezePlayheadSpeed.get() : jmap(freezePlayheadSpeed.get(), 1.0f, 5000.0f, 0.0f, 1.0f); }
        else if (name == "filterCutoff") { return !normed ? filterCutoff.get() : jmap(filterCutoff.get(), 10.0f, 20000.0f, 0.0f, 1.0f); }
        else if (name == "filterRessonance") { return !normed ? filterRessonance.get() : jmap(filterRessonance.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (name == "filterKeyboardTracking") { return !normed ? filterKeyboardTracking.get() : jmap(filterKeyboardTracking.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (name == "filterA") { return !normed ? filterA.get() : jmap(filterA.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (name == "filterD") { return !normed ? filterD.get() : jmap(filterD.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (name == "filterS") { return !normed ? filterS.get() : jmap(filterS.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (name == "filterR") { return !normed ? filterR.get() : jmap(filterR.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (name == "filterADSR2CutoffAmt") { return !normed ? filterADSR2CutoffAmt.get() : jmap(filterADSR2CutoffAmt.get(), 0.0f, 100.0f, 0.0f, 1.0f); }
        else if (name == "gain") { return !normed ? gain.get() : jmap(gain.get(), -80.0f, 12.0f, 0.0f, 1.0f); }
        else if (name == "ampA") { return !normed ? ampA.get() : jmap(ampA.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (name == "ampD") { return !normed ? ampD.get() : jmap(ampD.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (name == "ampS") { return !normed ? ampS.get() : jmap(ampS.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (name == "ampR") { return !normed ? ampR.get() : jmap(ampR.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (name == "pan") { return !normed ? pan.get() : jmap(pan.get(), -1.0f, 1.0f, 0.0f, 1.0f); }
        else if (name == "pitch") { return !normed ? pitch.get() : jmap(pitch.get(), -36.0f, 36.0f, 0.0f, 1.0f); }
        else if (name == "pitchBendRangeUp") { return !normed ? pitchBendRangeUp.get() : jmap(pitchBendRangeUp.get(), 0.0f, 36.0f, 0.0f, 1.0f); }
        else if (name == "pitchBendRangeDown") { return !normed ? pitchBendRangeDown.get() : jmap(pitchBendRangeDown.get(), 0.0f, 36.0f, 0.0f, 1.0f); }
        else if (name == "mod2CutoffAmt") { return !normed ? mod2CutoffAmt.get() : jmap(mod2CutoffAmt.get(), 0.0f, 100.0f, 0.0f, 1.0f); }
        else if (name == "mod2GainAmt") { return !normed ? mod2GainAmt.get() : jmap(mod2GainAmt.get(), -12.0f, 12.0f, 0.0f, 1.0f); }
        else if (name == "mod2PitchAmt") { return !normed ? mod2PitchAmt.get() : jmap(mod2PitchAmt.get(), -12.0f, 12.0f, 0.0f, 1.0f); }
        else if (name == "mod2PlayheadPos") { return !normed ? mod2PlayheadPos.get() : jmap(mod2PlayheadPos.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (name == "vel2CutoffAmt") { return !normed ? vel2CutoffAmt.get() : jmap(vel2CutoffAmt.get(), 0.0f, 100.0f, 0.0f, 1.0f); }
        else if (name == "vel2GainAmt") { return !normed ? vel2GainAmt.get() : jmap(vel2GainAmt.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        // --> End auto-generated code F
        throw std::runtime_error("No float parameter with this name");
    }
    
    void setParameterByNameFloat(const String& name, float value, bool normed){
        // --> Start auto-generated code B
        if (name == "startPosition") { startPosition = !normed ? jlimit(0.0f, 1.0f, value) : jmap(value, 0.0f, 1.0f); }
        else if (name == "endPosition") { endPosition = !normed ? jlimit(0.0f, 1.0f, value) : jmap(value, 0.0f, 1.0f); }
        else if (name == "loopStartPosition") { loopStartPosition = !normed ? jlimit(0.0f, 1.0f, value) : jmap(value, 0.0f, 1.0f); }
        else if (name == "loopEndPosition") { loopEndPosition = !normed ? jlimit(0.0f, 1.0f, value) : jmap(value, 0.0f, 1.0f); }
        else if (name == "playheadPosition") { playheadPosition = !normed ? jlimit(0.0f, 1.0f, value) : jmap(value, 0.0f, 1.0f); }
        else if (name == "freezePlayheadSpeed") { freezePlayheadSpeed = !normed ? jlimit(1.0f, 5000.0f, value) : jmap(value, 1.0f, 5000.0f); }
        else if (name == "filterCutoff") { filterCutoff = !normed ? jlimit(10.0f, 20000.0f, value) : jmap(value, 10.0f, 20000.0f); }
        else if (name == "filterRessonance") { filterRessonance = !normed ? jlimit(0.0f, 1.0f, value) : jmap(value, 0.0f, 1.0f); }
        else if (name == "filterKeyboardTracking") { filterKeyboardTracking = !normed ? jlimit(0.0f, 1.0f, value) : jmap(value, 0.0f, 1.0f); }
        else if (name == "filterA") { filterA = !normed ? jlimit(0.0f, 1.0f, value) : jmap(value, 0.0f, 1.0f); }
        else if (name == "filterD") { filterD = !normed ? jlimit(0.0f, 1.0f, value) : jmap(value, 0.0f, 1.0f); }
        else if (name == "filterS") { filterS = !normed ? jlimit(0.0f, 1.0f, value) : jmap(value, 0.0f, 1.0f); }
        else if (name == "filterR") { filterR = !normed ? jlimit(0.0f, 1.0f, value) : jmap(value, 0.0f, 1.0f); }
        else if (name == "filterADSR2CutoffAmt") { filterADSR2CutoffAmt = !normed ? jlimit(0.0f, 100.0f, value) : jmap(value, 0.0f, 100.0f); }
        else if (name == "gain") { gain = !normed ? jlimit(-80.0f, 12.0f, value) : jmap(value, -80.0f, 12.0f); }
        else if (name == "ampA") { ampA = !normed ? jlimit(0.0f, 1.0f, value) : jmap(value, 0.0f, 1.0f); }
        else if (name == "ampD") { ampD = !normed ? jlimit(0.0f, 1.0f, value) : jmap(value, 0.0f, 1.0f); }
        else if (name == "ampS") { ampS = !normed ? jlimit(0.0f, 1.0f, value) : jmap(value, 0.0f, 1.0f); }
        else if (name == "ampR") { ampR = !normed ? jlimit(0.0f, 1.0f, value) : jmap(value, 0.0f, 1.0f); }
        else if (name == "pan") { pan = !normed ? jlimit(-1.0f, 1.0f, value) : jmap(value, -1.0f, 1.0f); }
        else if (name == "pitch") { pitch = !normed ? jlimit(-36.0f, 36.0f, value) : jmap(value, -36.0f, 36.0f); }
        else if (name == "pitchBendRangeUp") { pitchBendRangeUp = !normed ? jlimit(0.0f, 36.0f, value) : jmap(value, 0.0f, 36.0f); }
        else if (name == "pitchBendRangeDown") { pitchBendRangeDown = !normed ? jlimit(0.0f, 36.0f, value) : jmap(value, 0.0f, 36.0f); }
        else if (name == "mod2CutoffAmt") { mod2CutoffAmt = !normed ? jlimit(0.0f, 100.0f, value) : jmap(value, 0.0f, 100.0f); }
        else if (name == "mod2GainAmt") { mod2GainAmt = !normed ? jlimit(-12.0f, 12.0f, value) : jmap(value, -12.0f, 12.0f); }
        else if (name == "mod2PitchAmt") { mod2PitchAmt = !normed ? jlimit(-12.0f, 12.0f, value) : jmap(value, -12.0f, 12.0f); }
        else if (name == "mod2PlayheadPos") { mod2PlayheadPos = !normed ? jlimit(0.0f, 1.0f, value) : jmap(value, 0.0f, 1.0f); }
        else if (name == "vel2CutoffAmt") { vel2CutoffAmt = !normed ? jlimit(0.0f, 100.0f, value) : jmap(value, 0.0f, 100.0f); }
        else if (name == "vel2GainAmt") { vel2GainAmt = !normed ? jlimit(0.0f, 1.0f, value) : jmap(value, 0.0f, 1.0f); }
        // --> End auto-generated code B
        else { throw std::runtime_error("No float parameter with this name"); }
        
        // Do some checking of start/end loop start/end positions to make sure we don't do anything wrong
        if (endPosition < startPosition) {
            endPosition = startPosition.get();
        }
        if (loopStartPosition < startPosition){
            loopStartPosition = startPosition.get();
        }
        if (loopEndPosition > endPosition){
            loopEndPosition = endPosition.get();
        }
        if (loopStartPosition > loopEndPosition){
            loopStartPosition = loopEndPosition.get();
        }
    }
    
    void setParameterByNameInt(const String& name, int value){
        // --> Start auto-generated code D
        if (name == "launchMode") { launchMode = jlimit(0, 4, value); }
        else if (name == "loopXFadeNSamples") { loopXFadeNSamples = jlimit(10, 100000, value); }
        else if (name == "reverse") { reverse = jlimit(0, 1, value); }
        else if (name == "noteMappingMode") { noteMappingMode = jlimit(0, 3, value); }
        else if (name == "numSlices") { numSlices = jlimit(0, 100, value); }
        else if (name == "midiRootNote") { midiRootNote = jlimit(0, 127, value); }
        // --> End auto-generated code D
        else { throw std::runtime_error("No int parameter with this name"); }
    }
    
    // ------------------------------------------------------------------------------------------------
    
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
    
    
    
    void run(){
        // TODO: use thread to download and load sounds (?)
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
    juce::CachedValue<float> filterA;
    juce::CachedValue<float> filterD;
    juce::CachedValue<float> filterS;
    juce::CachedValue<float> filterR;
    juce::CachedValue<float> filterADSR2CutoffAmt;
    juce::CachedValue<float> gain;
    juce::CachedValue<float> ampA;
    juce::CachedValue<float> ampD;
    juce::CachedValue<float> ampS;
    juce::CachedValue<float> ampR;
    juce::CachedValue<float> pan;
    juce::CachedValue<int> midiRootNote;
    juce::CachedValue<float> pitch;
    juce::CachedValue<float> pitchBendRangeUp;
    juce::CachedValue<float> pitchBendRangeDown;
    juce::CachedValue<float> mod2CutoffAmt;
    juce::CachedValue<float> mod2GainAmt;
    juce::CachedValue<float> mod2PitchAmt;
    juce::CachedValue<float> mod2PlayheadPos;
    juce::CachedValue<float> vel2CutoffAmt;
    juce::CachedValue<float> vel2GainAmt;
    // --> End auto-generated code A
    
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
