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
    SourceSamplerSound (const juce::ValueTree& _state,
                        SourceSound* _sourceSoundPointer,
                        AudioFormatReader& source,
                        bool _loadingPreviewVersion,
                        double maxSampleLengthSeconds,
                        double _pluginSampleRate,
                        int _pluginBlockSize);
    
    ~SourceSamplerSound() override;
    
    juce::ValueTree state;
    void bindState ();
    
    SourceSound* getSourceSound() { return sourceSoundPointer; };
   
    AudioBuffer<float>* getAudioData() const noexcept { return data.get(); }
    
    int getSoundId() { return soundId.get(); };
    bool getLoadedPreviewVersion();

    //==============================================================================
    bool appliesToNote (int midiNoteNumber) override;
    bool appliesToChannel (int midiChannel) override;
    
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
    void setMidiRootNote(int newMidiRootNote);
    
    //==============================================================================
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
    juce::CachedValue<juce::String> midiNotesAsString;

    // "Volatile" properties that are created when creating a sound
    std::unique_ptr<juce::AudioBuffer<float>> data;
    std::vector<int> onsetTimesSamples = {};
    bool loadedPreviewVersion = false;
    int lengthInSamples = 0;
    double soundSampleRate;
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
        soundType.referTo(state, IDs::soundType, nullptr, Defaults::soundType);
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
        filterAttack.referTo(state, IDs::filterAttack, nullptr, Defaults::filterAttack);
        filterDecay.referTo(state, IDs::filterDecay, nullptr, Defaults::filterDecay);
        filterSustain.referTo(state, IDs::filterSustain, nullptr, Defaults::filterSustain);
        filterRelease.referTo(state, IDs::filterRelease, nullptr, Defaults::filterRelease);
        filterADSR2CutoffAmt.referTo(state, IDs::filterADSR2CutoffAmt, nullptr, Defaults::filterADSR2CutoffAmt);
        gain.referTo(state, IDs::gain, nullptr, Defaults::gain);
        attack.referTo(state, IDs::attack, nullptr, Defaults::attack);
        decay.referTo(state, IDs::decay, nullptr, Defaults::decay);
        sustain.referTo(state, IDs::sustain, nullptr, Defaults::sustain);
        release.referTo(state, IDs::release, nullptr, Defaults::release);
        pan.referTo(state, IDs::pan, nullptr, Defaults::pan);
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
        
        midiCCmappings = std::make_unique<MidiCCMappingList>(state);
    }
    
    std::vector<SourceSamplerSound*> getLinkedSourceSamplerSounds() {
        std::vector<SourceSamplerSound*> objects;
        for (int i=0; i<getGlobalContext().sampler->getNumSounds(); i++){
            auto* sourceSamplerSound = static_cast<SourceSamplerSound*>(getGlobalContext().sampler->getSound(i).get());
            if (sourceSamplerSound->getSourceSound() == this){
                objects.push_back(sourceSamplerSound);
            }
        }
        return objects;
    }
    
    SourceSamplerSound* getFirstLinkedSourceSamplerSound() {
        std::vector<SourceSamplerSound*> objects;
        for (int i=0; i<getGlobalContext().sampler->getNumSounds(); i++){
            auto* sourceSamplerSound = static_cast<SourceSamplerSound*>(getGlobalContext().sampler->getSound(i).get());
            if (sourceSamplerSound->getSourceSound() == this){
                return sourceSamplerSound;
            }
        }
        return nullptr;
    }
    
    // --------------------------------------------------------------------------------------------
    
    juce::String getName() {
        return name.get();
    }
    
    juce::String getUUID() {
        return state.getProperty(IDs::uuid).toString();
    }
    
    bool isEnabled() {
        return enabled.get();
    }
    
    int getParameterInt(juce::Identifier identifier){
        // --> Start auto-generated code E
        if (identifier == IDs::soundType) { return soundType.get(); }
        else if (identifier == IDs::launchMode) { return launchMode.get(); }
        else if (identifier == IDs::loopXFadeNSamples) { return loopXFadeNSamples.get(); }
        else if (identifier == IDs::reverse) { return reverse.get(); }
        else if (identifier == IDs::noteMappingMode) { return noteMappingMode.get(); }
        else if (identifier == IDs::numSlices) { return numSlices.get(); }
        // --> End auto-generated code E
        throw std::runtime_error("No int parameter with this name");
    }
    
    float getParameterFloat(juce::Identifier identifier, bool normed){
        // --> Start auto-generated code F
        if (identifier == IDs::startPosition) { return !normed ? startPosition.get() : jmap(startPosition.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (identifier == IDs::endPosition) { return !normed ? endPosition.get() : jmap(endPosition.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (identifier == IDs::loopStartPosition) { return !normed ? loopStartPosition.get() : jmap(loopStartPosition.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (identifier == IDs::loopEndPosition) { return !normed ? loopEndPosition.get() : jmap(loopEndPosition.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (identifier == IDs::playheadPosition) { return !normed ? playheadPosition.get() : jmap(playheadPosition.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (identifier == IDs::freezePlayheadSpeed) { return !normed ? freezePlayheadSpeed.get() : jmap(freezePlayheadSpeed.get(), 1.0f, 5000.0f, 0.0f, 1.0f); }
        else if (identifier == IDs::filterCutoff) { return !normed ? filterCutoff.get() : jmap(filterCutoff.get(), 10.0f, 20000.0f, 0.0f, 1.0f); }
        else if (identifier == IDs::filterRessonance) { return !normed ? filterRessonance.get() : jmap(filterRessonance.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (identifier == IDs::filterKeyboardTracking) { return !normed ? filterKeyboardTracking.get() : jmap(filterKeyboardTracking.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (identifier == IDs::filterAttack) { return !normed ? filterAttack.get() : jmap(filterAttack.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (identifier == IDs::filterDecay) { return !normed ? filterDecay.get() : jmap(filterDecay.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (identifier == IDs::filterSustain) { return !normed ? filterSustain.get() : jmap(filterSustain.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (identifier == IDs::filterRelease) { return !normed ? filterRelease.get() : jmap(filterRelease.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (identifier == IDs::filterADSR2CutoffAmt) { return !normed ? filterADSR2CutoffAmt.get() : jmap(filterADSR2CutoffAmt.get(), 0.0f, 100.0f, 0.0f, 1.0f); }
        else if (identifier == IDs::gain) { return !normed ? gain.get() : jmap(gain.get(), -80.0f, 12.0f, 0.0f, 1.0f); }
        else if (identifier == IDs::attack) { return !normed ? attack.get() : jmap(attack.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (identifier == IDs::decay) { return !normed ? decay.get() : jmap(decay.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (identifier == IDs::sustain) { return !normed ? sustain.get() : jmap(sustain.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (identifier == IDs::release) { return !normed ? release.get() : jmap(release.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (identifier == IDs::pan) { return !normed ? pan.get() : jmap(pan.get(), -1.0f, 1.0f, 0.0f, 1.0f); }
        else if (identifier == IDs::pitch) { return !normed ? pitch.get() : jmap(pitch.get(), -36.0f, 36.0f, 0.0f, 1.0f); }
        else if (identifier == IDs::pitchBendRangeUp) { return !normed ? pitchBendRangeUp.get() : jmap(pitchBendRangeUp.get(), 0.0f, 36.0f, 0.0f, 1.0f); }
        else if (identifier == IDs::pitchBendRangeDown) { return !normed ? pitchBendRangeDown.get() : jmap(pitchBendRangeDown.get(), 0.0f, 36.0f, 0.0f, 1.0f); }
        else if (identifier == IDs::mod2CutoffAmt) { return !normed ? mod2CutoffAmt.get() : jmap(mod2CutoffAmt.get(), 0.0f, 100.0f, 0.0f, 1.0f); }
        else if (identifier == IDs::mod2GainAmt) { return !normed ? mod2GainAmt.get() : jmap(mod2GainAmt.get(), -12.0f, 12.0f, 0.0f, 1.0f); }
        else if (identifier == IDs::mod2PitchAmt) { return !normed ? mod2PitchAmt.get() : jmap(mod2PitchAmt.get(), -12.0f, 12.0f, 0.0f, 1.0f); }
        else if (identifier == IDs::mod2PlayheadPos) { return !normed ? mod2PlayheadPos.get() : jmap(mod2PlayheadPos.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (identifier == IDs::vel2CutoffAmt) { return !normed ? vel2CutoffAmt.get() : jmap(vel2CutoffAmt.get(), 0.0f, 100.0f, 0.0f, 1.0f); }
        else if (identifier == IDs::vel2GainAmt) { return !normed ? vel2GainAmt.get() : jmap(vel2GainAmt.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        // --> End auto-generated code F
        throw std::runtime_error("No float parameter with this name");
    }
    
    void setParameterByNameFloat(juce::Identifier identifier, float value, bool normed){
        // --> Start auto-generated code B
        if (identifier == IDs::startPosition) { startPosition = !normed ? jlimit(0.0f, 1.0f, value) : jmap(value, 0.0f, 1.0f); }
        else if (identifier == IDs::endPosition) { endPosition = !normed ? jlimit(0.0f, 1.0f, value) : jmap(value, 0.0f, 1.0f); }
        else if (identifier == IDs::loopStartPosition) { loopStartPosition = !normed ? jlimit(0.0f, 1.0f, value) : jmap(value, 0.0f, 1.0f); }
        else if (identifier == IDs::loopEndPosition) { loopEndPosition = !normed ? jlimit(0.0f, 1.0f, value) : jmap(value, 0.0f, 1.0f); }
        else if (identifier == IDs::playheadPosition) { playheadPosition = !normed ? jlimit(0.0f, 1.0f, value) : jmap(value, 0.0f, 1.0f); }
        else if (identifier == IDs::freezePlayheadSpeed) { freezePlayheadSpeed = !normed ? jlimit(1.0f, 5000.0f, value) : jmap(value, 1.0f, 5000.0f); }
        else if (identifier == IDs::filterCutoff) { filterCutoff = !normed ? jlimit(10.0f, 20000.0f, value) : jmap(value, 10.0f, 20000.0f); }
        else if (identifier == IDs::filterRessonance) { filterRessonance = !normed ? jlimit(0.0f, 1.0f, value) : jmap(value, 0.0f, 1.0f); }
        else if (identifier == IDs::filterKeyboardTracking) { filterKeyboardTracking = !normed ? jlimit(0.0f, 1.0f, value) : jmap(value, 0.0f, 1.0f); }
        else if (identifier == IDs::filterAttack) { filterAttack = !normed ? jlimit(0.0f, 1.0f, value) : jmap(value, 0.0f, 1.0f); }
        else if (identifier == IDs::filterDecay) { filterDecay = !normed ? jlimit(0.0f, 1.0f, value) : jmap(value, 0.0f, 1.0f); }
        else if (identifier == IDs::filterSustain) { filterSustain = !normed ? jlimit(0.0f, 1.0f, value) : jmap(value, 0.0f, 1.0f); }
        else if (identifier == IDs::filterRelease) { filterRelease = !normed ? jlimit(0.0f, 1.0f, value) : jmap(value, 0.0f, 1.0f); }
        else if (identifier == IDs::filterADSR2CutoffAmt) { filterADSR2CutoffAmt = !normed ? jlimit(0.0f, 100.0f, value) : jmap(value, 0.0f, 100.0f); }
        else if (identifier == IDs::gain) { gain = !normed ? jlimit(-80.0f, 12.0f, value) : jmap(value, -80.0f, 12.0f); }
        else if (identifier == IDs::attack) { attack = !normed ? jlimit(0.0f, 1.0f, value) : jmap(value, 0.0f, 1.0f); }
        else if (identifier == IDs::decay) { decay = !normed ? jlimit(0.0f, 1.0f, value) : jmap(value, 0.0f, 1.0f); }
        else if (identifier == IDs::sustain) { sustain = !normed ? jlimit(0.0f, 1.0f, value) : jmap(value, 0.0f, 1.0f); }
        else if (identifier == IDs::release) { release = !normed ? jlimit(0.0f, 1.0f, value) : jmap(value, 0.0f, 1.0f); }
        else if (identifier == IDs::pan) { pan = !normed ? jlimit(-1.0f, 1.0f, value) : jmap(value, -1.0f, 1.0f); }
        else if (identifier == IDs::pitch) { pitch = !normed ? jlimit(-36.0f, 36.0f, value) : jmap(value, -36.0f, 36.0f); }
        else if (identifier == IDs::pitchBendRangeUp) { pitchBendRangeUp = !normed ? jlimit(0.0f, 36.0f, value) : jmap(value, 0.0f, 36.0f); }
        else if (identifier == IDs::pitchBendRangeDown) { pitchBendRangeDown = !normed ? jlimit(0.0f, 36.0f, value) : jmap(value, 0.0f, 36.0f); }
        else if (identifier == IDs::mod2CutoffAmt) { mod2CutoffAmt = !normed ? jlimit(0.0f, 100.0f, value) : jmap(value, 0.0f, 100.0f); }
        else if (identifier == IDs::mod2GainAmt) { mod2GainAmt = !normed ? jlimit(-12.0f, 12.0f, value) : jmap(value, -12.0f, 12.0f); }
        else if (identifier == IDs::mod2PitchAmt) { mod2PitchAmt = !normed ? jlimit(-12.0f, 12.0f, value) : jmap(value, -12.0f, 12.0f); }
        else if (identifier == IDs::mod2PlayheadPos) { mod2PlayheadPos = !normed ? jlimit(0.0f, 1.0f, value) : jmap(value, 0.0f, 1.0f); }
        else if (identifier == IDs::vel2CutoffAmt) { vel2CutoffAmt = !normed ? jlimit(0.0f, 100.0f, value) : jmap(value, 0.0f, 100.0f); }
        else if (identifier == IDs::vel2GainAmt) { vel2GainAmt = !normed ? jlimit(0.0f, 1.0f, value) : jmap(value, 0.0f, 1.0f); }
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
    
    void setParameterByNameInt(juce::Identifier identifier, int value){
        // --> Start auto-generated code D
        if (identifier == IDs::soundType) { soundType = jlimit(0, 1, value); }
        else if (identifier == IDs::launchMode) { launchMode = jlimit(0, 4, value); }
        else if (identifier == IDs::loopXFadeNSamples) { loopXFadeNSamples = jlimit(10, 100000, value); }
        else if (identifier == IDs::reverse) { reverse = jlimit(0, 1, value); }
        else if (identifier == IDs::noteMappingMode) { noteMappingMode = jlimit(0, 3, value); }
        else if (identifier == IDs::numSlices) { numSlices = jlimit(0, 100, value); }
        // --> End auto-generated code D
        else { throw std::runtime_error("No int parameter with this name"); }
    }
    
    juce::BigInteger getMappedMidiNotes(){
        // Get the mapped midi notes of the first sound
        // NOTE: this method should not be called for multi-sample sounds (should I assert here?)
        SourceSamplerSound* first = getFirstLinkedSourceSamplerSound();
        if (first != nullptr){
            return first->getMappedMidiNotes();
        }
        juce::BigInteger defaultValue;
        defaultValue.setRange(0, 127, true);
        return defaultValue;
    }
    
    int getNumberOfMappedMidiNotes(){
        // Get the mapped midi notes of the first sound
        // NOTE: this method should not be called for multi-sample sounds (should I assert here?)
        return getMappedMidiNotes().countNumberOfSetBits();
    }
    
    void setMappedMidiNotes(juce::BigInteger newMappedMidiNotes){
        // Set the mapped midi notes to all of the sampler sounds
        // NOTE: this method should not be called for multi-sample sounds (should I assert here?)
        for (auto sourceSamplerSound: getLinkedSourceSamplerSounds()){
            sourceSamplerSound->setMappedMidiNotes(newMappedMidiNotes);
        }
    }
    
    int getMidiRootNote(){
        // Get the midi root note of the first sound
        // NOTE: this method should not be called for multi-sample sounds (should I assert here?)
        SourceSamplerSound* first = getFirstLinkedSourceSamplerSound();
        if (first != nullptr){
            return first->getMidiRootNote();
        }
        return -1;
    }
    
    void setMidiRootNote(int newMidiRootNote){
        // Set the midi root note to all of the sampler sounds
        // NOTE: this method should not be called for multi-sample sounds (should I assert here?)
        for (auto sourceSamplerSound: getLinkedSourceSamplerSounds()){
            sourceSamplerSound->setMidiRootNote(newMidiRootNote);
        }
    }
    
    // ------------------------------------------------------------------------------------------------
    
    void addOrEditMidiMapping(juce::String uuid, int ccNumber, String parameterName, float minRange, float maxRange){
        
        ValueTree existingMapping = state.getChildWithProperty(IDs::uuid, uuid);
        if (existingMapping.isValid()) {
            // Modify existing mapping
            existingMapping.setProperty (IDs::ccNumber, ccNumber, nullptr);
            existingMapping.setProperty (IDs::parameterName, parameterName, nullptr);
            existingMapping.setProperty (IDs::minRange, jlimit(0.0f, 1.0f, minRange), nullptr);
            existingMapping.setProperty (IDs::maxRange, jlimit(0.0f, 1.0f, maxRange), nullptr);
        } else {
            // No mapping with uuid found, create a new one
            // TODO: move this to the MidiCCMappingList class?
            ValueTree newMapping = Helpers::createMidiMappingState(ccNumber, parameterName, jlimit(0.0f, 1.0f, minRange), jlimit(0.0f, 1.0f, maxRange));
            state.addChild(newMapping, -1, nullptr);
        }
    }

    std::vector<MidiCCMapping*> getMidiMappingsForCcNumber(int ccNumber){
        // TODO: transofrm to use pointer
        std::vector<MidiCCMapping*> toReturn = {};
        for (auto* midiCCmapping: midiCCmappings->objects){
            if (midiCCmapping->ccNumber == ccNumber){
                toReturn.push_back(midiCCmapping);
            }
        }
        return toReturn;
    }

    void removeMidiMapping(juce::String uuid){
        midiCCmappings->removeMidiCCMappingWithUUID(uuid);
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
                    SourceSamplerSound* createdSound = new SourceSamplerSound(child,
                                                                              this,
                                                                              *reader,
                                                                              true,
                                                                              MAX_SAMPLE_LENGTH,
                                                                              getGlobalContext().sampleRate,
                                                                              getGlobalContext().samplesPerBlock);
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
        for (auto sourceSamplerSound: sourceSamplerSounds) { getGlobalContext().sampler->addSound(sourceSamplerSound); }
        std::cout << "Added " << sourceSamplerSounds.size() << " SourceSamplerSound(s) to sampler... " << std::endl;
    }
    
    void removeSourceSampleSoundsFromSampler()
    {
        // TODO: add lock here?
        
        std::vector<int> soundIndexesToDelete;
        for (int i=0; i<getGlobalContext().sampler->getNumSounds(); i++){
            auto* sourceSamplerSound = static_cast<SourceSamplerSound*>(getGlobalContext().sampler->getSound(i).get());
            if (sourceSamplerSound->getSourceSound() == this){
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
    
    std::unique_ptr<MidiCCMappingList> midiCCmappings;

    // --> Start auto-generated code A
    juce::CachedValue<int> soundType;
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
    
    SourceSound* getSoundAt(int position) {
        if ((position >= 0) && (position < objects.size() - 1)){
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
};
