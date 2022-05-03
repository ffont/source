/*
  ==============================================================================

    SourceSamplerSound.cpp
    Created: 23 Mar 2022 1:38:02pm
    Author:  Frederic Font Corbera

  ==============================================================================
*/

#include "SourceSamplerSound.h"
#include "SourceSamplerVoice.h"


SourceSamplerSound::SourceSamplerSound (const juce::ValueTree& _state,
                                        SourceSound* _sourceSoundPointer,
                                        juce::AudioFormatReader& source,
                                        double maxSampleLengthSeconds,
                                        double _pluginSampleRate,
                                        int _pluginBlockSize)
    : state(_state),
      soundSampleRate (source.sampleRate),
      pluginSampleRate (_pluginSampleRate),
      pluginBlockSize (_pluginBlockSize)
{
    sourceSoundPointer = _sourceSoundPointer;
    bindState();
    
    // Load audio
    if (soundSampleRate > 0 && source.lengthInSamples > 0) {
        lengthInSamples = juce::jmin ((int) source.lengthInSamples, (int) (maxSampleLengthSeconds * soundSampleRate));
        data.reset (new juce::AudioBuffer<float> (juce::jmin (2, (int) source.numChannels), lengthInSamples + 4));
        source.read (data.get(), 0, lengthInSamples + 4, 0, true, true);
        
        // Add duration to state
        state.setProperty(IDs::duration, getLengthInSeconds(), nullptr);
    }
    
    // Load calculated onsets (if any)
    loadOnsetTimesSamplesFromAnalysis();
    
    // Write PCM version of the audio to disk so it can be used in the UI for displaying waveforms
    // (either by serving through the http server or directly loading from disk)
    writeBufferToDisk();
}

SourceSamplerSound::~SourceSamplerSound()
{
}

void SourceSamplerSound::bindState ()
{
    Helpers::addPropertyWithDefaultValueIfNotExisting(state, IDs::name, Defaults::name);
    name.referTo(state, IDs::name, nullptr);
    Helpers::addPropertyWithDefaultValueIfNotExisting(state, IDs::soundId, Defaults::soundId);
    soundId.referTo(state, IDs::soundId, nullptr);
    Helpers::addPropertyWithDefaultValueIfNotExisting(state, IDs::midiRootNote, Defaults::midiRootNote);
    midiRootNote.referTo(state, IDs::midiRootNote, nullptr);
    Helpers::addPropertyWithDefaultValueIfNotExisting(state, IDs::midiVelocityLayer, Defaults::midiVelocityLayer);
    midiVelocityLayer.referTo(state, IDs::midiVelocityLayer, nullptr);
}

void SourceSamplerSound::writeBufferToDisk()
{
    juce::AudioBuffer<float>* buffer = getAudioData();
    juce::WavAudioFormat format;
    std::unique_ptr<juce::AudioFormatWriter> writer;
    
    juce::File outputLocation = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("SourceSampler/tmp/" + state.getProperty(IDs::uuid).toString()).withFileExtension("wav");
    writer.reset (format.createWriterFor (new juce::FileOutputStream (outputLocation),
                                          soundSampleRate,
                                          1,  // Write mono files for visualization (will probably only take the first channel)
                                          16,
                                          {},
                                          0));
    if (writer != nullptr)
        writer->writeFromAudioSampleBuffer (*buffer, 0, buffer->getNumSamples());
}

bool SourceSamplerSound::appliesToNote (int midiNoteNumber)
{
    // If sound is disabled, return false so new notes are not triggered
    return !sourceSoundPointer->isScheduledForDeletion() && getMappedMidiNotes()[midiNoteNumber];
}

bool SourceSamplerSound::appliesToVelocity (int midiVelocity)
{
    return midiVelocities[midiVelocity];
}

bool SourceSamplerSound::appliesToChannel (int midiChannel)
{
    int soundMidiChannel = getParameterInt(IDs::midiChannel);
    if (soundMidiChannel == 0){
        // use global channel
        int globalMidiChannel = sourceSoundPointer->getGlobalContext().midiInChannel;
        return globalMidiChannel == 0 || globalMidiChannel == midiChannel;
    } else {
        // use the sound channel
        return soundMidiChannel == midiChannel;
    }
}

float SourceSamplerSound::getParameterFloat(juce::Identifier identifier){
    return sourceSoundPointer->getParameterFloat(identifier, false);
}

int SourceSamplerSound::getParameterInt(juce::Identifier identifier){
    return sourceSoundPointer->getParameterInt(identifier);
}

int SourceSamplerSound::getLengthInSamples(){
    return lengthInSamples;
}

float SourceSamplerSound::getLengthInSeconds(){
    return (float)getLengthInSamples()/soundSampleRate;
}

int SourceSamplerSound::getNumberOfMappedMidiNotes()
{
    return getMappedMidiNotes().countNumberOfSetBits();
}

juce::BigInteger SourceSamplerSound::getMappedMidiNotes()
{
    return midiNotes;
}

void SourceSamplerSound::setMappedMidiNotes(juce::BigInteger newMappedMidiNotes)
{
    midiNotes = newMappedMidiNotes;
}

int SourceSamplerSound::getMidiRootNote()
{
    return midiRootNote.get();
}

void SourceSamplerSound::setMidiRootNote(int newRootNote)
{
    midiRootNote = newRootNote;
}

int SourceSamplerSound::getMidiVelocityLayer()
{
    return midiVelocityLayer.get();
}

void SourceSamplerSound::setMappedVelocities(juce::BigInteger newMappedVelocities)
{
    midiVelocities = newMappedVelocities;
}

void SourceSamplerSound::setOnsetTimesSamples(std::vector<float> onsetTimes){
    onsetTimesSamples.clear();
    for (int i=0; i<onsetTimes.size(); i++){
        onsetTimesSamples.push_back((int)(onsetTimes[i] * soundSampleRate));
    }
}

std::vector<int> SourceSamplerSound::getOnsetTimesSamples(){
    return onsetTimesSamples;
}

void SourceSamplerSound::loadOnsetTimesSamplesFromAnalysis(){
    juce::ValueTree soundAnalysis = state.getChildWithName(IDs::ANALYSIS);
    if (soundAnalysis.isValid()){
        juce::ValueTree onsetTimes = soundAnalysis.getChildWithName(IDs::onsets);
        if (onsetTimes.isValid()){
            std::vector<float> onsets = {};
            for (int i=0; i<onsetTimes.getNumChildren(); i++){
                juce::ValueTree onsetVT = onsetTimes.getChild(i);
                float onset = (float)(onsetVT.getProperty(IDs::onsetTime));
                onsets.push_back(onset);
            }
            setOnsetTimesSamples(onsets);
        }
    }
}


//==============================================================================


SourceSound::SourceSound (const juce::ValueTree& _state,
             std::function<GlobalContextStruct()> globalContextGetter): state(_state), soundLoaderThread (*this)
{
    getGlobalContext = globalContextGetter;
    bindState();
    
    // Trigger the loading of sounds in a separate thread
    soundLoaderThread.startThread();
}

SourceSound::~SourceSound ()
{
    for (int i = 0; i < downloadTasks.size(); i++) {
        downloadTasks.at(i).reset();
    }
}

void SourceSound::bindState ()
{
    Helpers::addPropertyWithDefaultValueIfNotExisting(state, IDs::willBeDeleted, Defaults::willBeDeleted);
    willBeDeleted.referTo(state, IDs::willBeDeleted, nullptr);
    Helpers::addPropertyWithDefaultValueIfNotExisting(state, IDs::allSoundsLoaded, Defaults::allSoundsLoaded);
    allSoundsLoaded.referTo(state, IDs::allSoundsLoaded, nullptr);
    Helpers::addPropertyWithDefaultValueIfNotExisting(state, IDs::midiNotes, Defaults::midiNotes);
    midiNotesAsString.referTo(state, IDs::midiNotes, nullptr);
    
    // --> Start auto-generated code C
    Helpers::addPropertyWithDefaultValueIfNotExisting(state, IDs::launchMode, Defaults::launchMode);
    launchMode.referTo(state, IDs::launchMode, nullptr, Defaults::launchMode);
    Helpers::addPropertyWithDefaultValueIfNotExisting(state, IDs::startPosition, Defaults::startPosition);
    startPosition.referTo(state, IDs::startPosition, nullptr, Defaults::startPosition);
    Helpers::addPropertyWithDefaultValueIfNotExisting(state, IDs::endPosition, Defaults::endPosition);
    endPosition.referTo(state, IDs::endPosition, nullptr, Defaults::endPosition);
    Helpers::addPropertyWithDefaultValueIfNotExisting(state, IDs::loopStartPosition, Defaults::loopStartPosition);
    loopStartPosition.referTo(state, IDs::loopStartPosition, nullptr, Defaults::loopStartPosition);
    Helpers::addPropertyWithDefaultValueIfNotExisting(state, IDs::loopEndPosition, Defaults::loopEndPosition);
    loopEndPosition.referTo(state, IDs::loopEndPosition, nullptr, Defaults::loopEndPosition);
    Helpers::addPropertyWithDefaultValueIfNotExisting(state, IDs::loopXFadeNSamples, Defaults::loopXFadeNSamples);
    loopXFadeNSamples.referTo(state, IDs::loopXFadeNSamples, nullptr, Defaults::loopXFadeNSamples);
    Helpers::addPropertyWithDefaultValueIfNotExisting(state, IDs::reverse, Defaults::reverse);
    reverse.referTo(state, IDs::reverse, nullptr, Defaults::reverse);
    Helpers::addPropertyWithDefaultValueIfNotExisting(state, IDs::noteMappingMode, Defaults::noteMappingMode);
    noteMappingMode.referTo(state, IDs::noteMappingMode, nullptr, Defaults::noteMappingMode);
    Helpers::addPropertyWithDefaultValueIfNotExisting(state, IDs::numSlices, Defaults::numSlices);
    numSlices.referTo(state, IDs::numSlices, nullptr, Defaults::numSlices);
    Helpers::addPropertyWithDefaultValueIfNotExisting(state, IDs::playheadPosition, Defaults::playheadPosition);
    playheadPosition.referTo(state, IDs::playheadPosition, nullptr, Defaults::playheadPosition);
    Helpers::addPropertyWithDefaultValueIfNotExisting(state, IDs::freezePlayheadSpeed, Defaults::freezePlayheadSpeed);
    freezePlayheadSpeed.referTo(state, IDs::freezePlayheadSpeed, nullptr, Defaults::freezePlayheadSpeed);
    Helpers::addPropertyWithDefaultValueIfNotExisting(state, IDs::filterCutoff, Defaults::filterCutoff);
    filterCutoff.referTo(state, IDs::filterCutoff, nullptr, Defaults::filterCutoff);
    Helpers::addPropertyWithDefaultValueIfNotExisting(state, IDs::filterRessonance, Defaults::filterRessonance);
    filterRessonance.referTo(state, IDs::filterRessonance, nullptr, Defaults::filterRessonance);
    Helpers::addPropertyWithDefaultValueIfNotExisting(state, IDs::filterKeyboardTracking, Defaults::filterKeyboardTracking);
    filterKeyboardTracking.referTo(state, IDs::filterKeyboardTracking, nullptr, Defaults::filterKeyboardTracking);
    Helpers::addPropertyWithDefaultValueIfNotExisting(state, IDs::filterAttack, Defaults::filterAttack);
    filterAttack.referTo(state, IDs::filterAttack, nullptr, Defaults::filterAttack);
    Helpers::addPropertyWithDefaultValueIfNotExisting(state, IDs::filterDecay, Defaults::filterDecay);
    filterDecay.referTo(state, IDs::filterDecay, nullptr, Defaults::filterDecay);
    Helpers::addPropertyWithDefaultValueIfNotExisting(state, IDs::filterSustain, Defaults::filterSustain);
    filterSustain.referTo(state, IDs::filterSustain, nullptr, Defaults::filterSustain);
    Helpers::addPropertyWithDefaultValueIfNotExisting(state, IDs::filterRelease, Defaults::filterRelease);
    filterRelease.referTo(state, IDs::filterRelease, nullptr, Defaults::filterRelease);
    Helpers::addPropertyWithDefaultValueIfNotExisting(state, IDs::filterADSR2CutoffAmt, Defaults::filterADSR2CutoffAmt);
    filterADSR2CutoffAmt.referTo(state, IDs::filterADSR2CutoffAmt, nullptr, Defaults::filterADSR2CutoffAmt);
    Helpers::addPropertyWithDefaultValueIfNotExisting(state, IDs::gain, Defaults::gain);
    gain.referTo(state, IDs::gain, nullptr, Defaults::gain);
    Helpers::addPropertyWithDefaultValueIfNotExisting(state, IDs::attack, Defaults::attack);
    attack.referTo(state, IDs::attack, nullptr, Defaults::attack);
    Helpers::addPropertyWithDefaultValueIfNotExisting(state, IDs::decay, Defaults::decay);
    decay.referTo(state, IDs::decay, nullptr, Defaults::decay);
    Helpers::addPropertyWithDefaultValueIfNotExisting(state, IDs::sustain, Defaults::sustain);
    sustain.referTo(state, IDs::sustain, nullptr, Defaults::sustain);
    Helpers::addPropertyWithDefaultValueIfNotExisting(state, IDs::release, Defaults::release);
    release.referTo(state, IDs::release, nullptr, Defaults::release);
    Helpers::addPropertyWithDefaultValueIfNotExisting(state, IDs::pan, Defaults::pan);
    pan.referTo(state, IDs::pan, nullptr, Defaults::pan);
    Helpers::addPropertyWithDefaultValueIfNotExisting(state, IDs::pitch, Defaults::pitch);
    pitch.referTo(state, IDs::pitch, nullptr, Defaults::pitch);
    Helpers::addPropertyWithDefaultValueIfNotExisting(state, IDs::pitchBendRangeUp, Defaults::pitchBendRangeUp);
    pitchBendRangeUp.referTo(state, IDs::pitchBendRangeUp, nullptr, Defaults::pitchBendRangeUp);
    Helpers::addPropertyWithDefaultValueIfNotExisting(state, IDs::pitchBendRangeDown, Defaults::pitchBendRangeDown);
    pitchBendRangeDown.referTo(state, IDs::pitchBendRangeDown, nullptr, Defaults::pitchBendRangeDown);
    Helpers::addPropertyWithDefaultValueIfNotExisting(state, IDs::mod2CutoffAmt, Defaults::mod2CutoffAmt);
    mod2CutoffAmt.referTo(state, IDs::mod2CutoffAmt, nullptr, Defaults::mod2CutoffAmt);
    Helpers::addPropertyWithDefaultValueIfNotExisting(state, IDs::mod2GainAmt, Defaults::mod2GainAmt);
    mod2GainAmt.referTo(state, IDs::mod2GainAmt, nullptr, Defaults::mod2GainAmt);
    Helpers::addPropertyWithDefaultValueIfNotExisting(state, IDs::mod2PitchAmt, Defaults::mod2PitchAmt);
    mod2PitchAmt.referTo(state, IDs::mod2PitchAmt, nullptr, Defaults::mod2PitchAmt);
    Helpers::addPropertyWithDefaultValueIfNotExisting(state, IDs::mod2PlayheadPos, Defaults::mod2PlayheadPos);
    mod2PlayheadPos.referTo(state, IDs::mod2PlayheadPos, nullptr, Defaults::mod2PlayheadPos);
    Helpers::addPropertyWithDefaultValueIfNotExisting(state, IDs::vel2CutoffAmt, Defaults::vel2CutoffAmt);
    vel2CutoffAmt.referTo(state, IDs::vel2CutoffAmt, nullptr, Defaults::vel2CutoffAmt);
    Helpers::addPropertyWithDefaultValueIfNotExisting(state, IDs::vel2GainAmt, Defaults::vel2GainAmt);
    vel2GainAmt.referTo(state, IDs::vel2GainAmt, nullptr, Defaults::vel2GainAmt);
    Helpers::addPropertyWithDefaultValueIfNotExisting(state, IDs::midiChannel, Defaults::midiChannel);
    midiChannel.referTo(state, IDs::midiChannel, nullptr, Defaults::midiChannel);
    // --> End auto-generated code C
    
    midiCCmappings = std::make_unique<MidiCCMappingList>(state);
}

std::vector<SourceSamplerSound*> SourceSound::getLinkedSourceSamplerSounds() {
    std::vector<SourceSamplerSound*> objects;
    for (int i=0; i<getGlobalContext().sampler->getNumSounds(); i++){
        auto* sourceSamplerSound = static_cast<SourceSamplerSound*>(getGlobalContext().sampler->getSound(i).get());
        if (sourceSamplerSound->getSourceSound() == this){
            objects.push_back(sourceSamplerSound);
        }
    }
    return objects;
}

SourceSamplerSound* SourceSound::getFirstLinkedSourceSamplerSound() {
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

juce::String SourceSound::getUUID() {
    return state.getProperty(IDs::uuid).toString();
}

bool SourceSound::isScheduledForDeletion() {
    return willBeDeleted.get();
}

void SourceSound::scheduleSoundDeletion(){
    // Trigger stop all currently active notes for that sound and set timestamp so sound gets deleted async
    // If sound already disabled, don't trigger deletion again
    willBeDeleted = true;
    scheduledForDeletionTime = juce::Time::getMillisecondCounterHiRes();
    
    // Stop all notes currently being played from that sound
    for (int i=0; i<getGlobalContext().sampler->getNumVoices(); i++){
        auto* voice = getGlobalContext().sampler->getVoice(i);
        if (voice != nullptr){
            if (voice->isVoiceActive()){
                auto* currentlyPlayingSound = static_cast<SourceSamplerVoice*>(voice)->getCurrentlyPlayingSourceSamplerSound();
                if (currentlyPlayingSound->getSourceSound()->getUUID() == getUUID()){
                    voice->stopNote(0.0f, false);
                }
            }
        }
    }
}

bool SourceSound::shouldBeDeleted(){
    return isScheduledForDeletion() && ((juce::Time::getMillisecondCounterHiRes() - scheduledForDeletionTime) > SAFE_SOUND_DELETION_TIME_MS);
}

// --------------------------------------------------------------------------------------------

int SourceSound::getParameterInt(juce::Identifier identifier){
    // --> Start auto-generated code E
        if (identifier == IDs::launchMode) { return launchMode.get(); }
        else if (identifier == IDs::loopXFadeNSamples) { return loopXFadeNSamples.get(); }
        else if (identifier == IDs::reverse) { return reverse.get(); }
        else if (identifier == IDs::noteMappingMode) { return noteMappingMode.get(); }
        else if (identifier == IDs::numSlices) { return numSlices.get(); }
        else if (identifier == IDs::midiChannel) { return midiChannel.get(); }
        // --> End auto-generated code E
    throw std::runtime_error("No int parameter with this name");
}

float SourceSound::getParameterFloat(juce::Identifier identifier, bool normed){
    // --> Start auto-generated code F
        if (identifier == IDs::startPosition) { return !normed ? startPosition.get() : juce::jmap(startPosition.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (identifier == IDs::endPosition) { return !normed ? endPosition.get() : juce::jmap(endPosition.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (identifier == IDs::loopStartPosition) { return !normed ? loopStartPosition.get() : juce::jmap(loopStartPosition.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (identifier == IDs::loopEndPosition) { return !normed ? loopEndPosition.get() : juce::jmap(loopEndPosition.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (identifier == IDs::playheadPosition) { return !normed ? playheadPosition.get() : juce::jmap(playheadPosition.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (identifier == IDs::freezePlayheadSpeed) { return !normed ? freezePlayheadSpeed.get() : juce::jmap(freezePlayheadSpeed.get(), 1.0f, 5000.0f, 0.0f, 1.0f); }
        else if (identifier == IDs::filterCutoff) { return !normed ? filterCutoff.get() : juce::jmap(filterCutoff.get(), 10.0f, 20000.0f, 0.0f, 1.0f); }
        else if (identifier == IDs::filterRessonance) { return !normed ? filterRessonance.get() : juce::jmap(filterRessonance.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (identifier == IDs::filterKeyboardTracking) { return !normed ? filterKeyboardTracking.get() : juce::jmap(filterKeyboardTracking.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (identifier == IDs::filterAttack) { return !normed ? filterAttack.get() : juce::jmap(filterAttack.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (identifier == IDs::filterDecay) { return !normed ? filterDecay.get() : juce::jmap(filterDecay.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (identifier == IDs::filterSustain) { return !normed ? filterSustain.get() : juce::jmap(filterSustain.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (identifier == IDs::filterRelease) { return !normed ? filterRelease.get() : juce::jmap(filterRelease.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (identifier == IDs::filterADSR2CutoffAmt) { return !normed ? filterADSR2CutoffAmt.get() : juce::jmap(filterADSR2CutoffAmt.get(), 0.0f, 100.0f, 0.0f, 1.0f); }
        else if (identifier == IDs::gain) { return !normed ? gain.get() : juce::jmap(gain.get(), -80.0f, 12.0f, 0.0f, 1.0f); }
        else if (identifier == IDs::attack) { return !normed ? attack.get() : juce::jmap(attack.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (identifier == IDs::decay) { return !normed ? decay.get() : juce::jmap(decay.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (identifier == IDs::sustain) { return !normed ? sustain.get() : juce::jmap(sustain.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (identifier == IDs::release) { return !normed ? release.get() : juce::jmap(release.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (identifier == IDs::pan) { return !normed ? pan.get() : juce::jmap(pan.get(), -1.0f, 1.0f, 0.0f, 1.0f); }
        else if (identifier == IDs::pitch) { return !normed ? pitch.get() : juce::jmap(pitch.get(), -36.0f, 36.0f, 0.0f, 1.0f); }
        else if (identifier == IDs::pitchBendRangeUp) { return !normed ? pitchBendRangeUp.get() : juce::jmap(pitchBendRangeUp.get(), 0.0f, 36.0f, 0.0f, 1.0f); }
        else if (identifier == IDs::pitchBendRangeDown) { return !normed ? pitchBendRangeDown.get() : juce::jmap(pitchBendRangeDown.get(), 0.0f, 36.0f, 0.0f, 1.0f); }
        else if (identifier == IDs::mod2CutoffAmt) { return !normed ? mod2CutoffAmt.get() : juce::jmap(mod2CutoffAmt.get(), 0.0f, 100.0f, 0.0f, 1.0f); }
        else if (identifier == IDs::mod2GainAmt) { return !normed ? mod2GainAmt.get() : juce::jmap(mod2GainAmt.get(), -12.0f, 12.0f, 0.0f, 1.0f); }
        else if (identifier == IDs::mod2PitchAmt) { return !normed ? mod2PitchAmt.get() : juce::jmap(mod2PitchAmt.get(), -12.0f, 12.0f, 0.0f, 1.0f); }
        else if (identifier == IDs::mod2PlayheadPos) { return !normed ? mod2PlayheadPos.get() : juce::jmap(mod2PlayheadPos.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (identifier == IDs::vel2CutoffAmt) { return !normed ? vel2CutoffAmt.get() : juce::jmap(vel2CutoffAmt.get(), 0.0f, 100.0f, 0.0f, 1.0f); }
        else if (identifier == IDs::vel2GainAmt) { return !normed ? vel2GainAmt.get() : juce::jmap(vel2GainAmt.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        // --> End auto-generated code F
    throw std::runtime_error("No float parameter with this name");
}

void SourceSound::setParameterByNameFloat(juce::Identifier identifier, float value, bool normed){
    // --> Start auto-generated code B
        if (identifier == IDs::startPosition) { startPosition = !normed ? juce::jlimit(0.0f, 1.0f, value) : juce::jmap(value, 0.0f, 1.0f); }
        else if (identifier == IDs::endPosition) { endPosition = !normed ? juce::jlimit(0.0f, 1.0f, value) : juce::jmap(value, 0.0f, 1.0f); }
        else if (identifier == IDs::loopStartPosition) { loopStartPosition = !normed ? juce::jlimit(0.0f, 1.0f, value) : juce::jmap(value, 0.0f, 1.0f); }
        else if (identifier == IDs::loopEndPosition) { loopEndPosition = !normed ? juce::jlimit(0.0f, 1.0f, value) : juce::jmap(value, 0.0f, 1.0f); }
        else if (identifier == IDs::playheadPosition) { playheadPosition = !normed ? juce::jlimit(0.0f, 1.0f, value) : juce::jmap(value, 0.0f, 1.0f); }
        else if (identifier == IDs::freezePlayheadSpeed) { freezePlayheadSpeed = !normed ? juce::jlimit(1.0f, 5000.0f, value) : juce::jmap(value, 1.0f, 5000.0f); }
        else if (identifier == IDs::filterCutoff) { filterCutoff = !normed ? juce::jlimit(10.0f, 20000.0f, value) : juce::jmap(value, 10.0f, 20000.0f); }
        else if (identifier == IDs::filterRessonance) { filterRessonance = !normed ? juce::jlimit(0.0f, 1.0f, value) : juce::jmap(value, 0.0f, 1.0f); }
        else if (identifier == IDs::filterKeyboardTracking) { filterKeyboardTracking = !normed ? juce::jlimit(0.0f, 1.0f, value) : juce::jmap(value, 0.0f, 1.0f); }
        else if (identifier == IDs::filterAttack) { filterAttack = !normed ? juce::jlimit(0.0f, 1.0f, value) : juce::jmap(value, 0.0f, 1.0f); }
        else if (identifier == IDs::filterDecay) { filterDecay = !normed ? juce::jlimit(0.0f, 1.0f, value) : juce::jmap(value, 0.0f, 1.0f); }
        else if (identifier == IDs::filterSustain) { filterSustain = !normed ? juce::jlimit(0.0f, 1.0f, value) : juce::jmap(value, 0.0f, 1.0f); }
        else if (identifier == IDs::filterRelease) { filterRelease = !normed ? juce::jlimit(0.0f, 1.0f, value) : juce::jmap(value, 0.0f, 1.0f); }
        else if (identifier == IDs::filterADSR2CutoffAmt) { filterADSR2CutoffAmt = !normed ? juce::jlimit(0.0f, 100.0f, value) : juce::jmap(value, 0.0f, 100.0f); }
        else if (identifier == IDs::gain) { gain = !normed ? juce::jlimit(-80.0f, 12.0f, value) : juce::jmap(value, -80.0f, 12.0f); }
        else if (identifier == IDs::attack) { attack = !normed ? juce::jlimit(0.0f, 1.0f, value) : juce::jmap(value, 0.0f, 1.0f); }
        else if (identifier == IDs::decay) { decay = !normed ? juce::jlimit(0.0f, 1.0f, value) : juce::jmap(value, 0.0f, 1.0f); }
        else if (identifier == IDs::sustain) { sustain = !normed ? juce::jlimit(0.0f, 1.0f, value) : juce::jmap(value, 0.0f, 1.0f); }
        else if (identifier == IDs::release) { release = !normed ? juce::jlimit(0.0f, 1.0f, value) : juce::jmap(value, 0.0f, 1.0f); }
        else if (identifier == IDs::pan) { pan = !normed ? juce::jlimit(-1.0f, 1.0f, value) : juce::jmap(value, -1.0f, 1.0f); }
        else if (identifier == IDs::pitch) { pitch = !normed ? juce::jlimit(-36.0f, 36.0f, value) : juce::jmap(value, -36.0f, 36.0f); }
        else if (identifier == IDs::pitchBendRangeUp) { pitchBendRangeUp = !normed ? juce::jlimit(0.0f, 36.0f, value) : juce::jmap(value, 0.0f, 36.0f); }
        else if (identifier == IDs::pitchBendRangeDown) { pitchBendRangeDown = !normed ? juce::jlimit(0.0f, 36.0f, value) : juce::jmap(value, 0.0f, 36.0f); }
        else if (identifier == IDs::mod2CutoffAmt) { mod2CutoffAmt = !normed ? juce::jlimit(0.0f, 100.0f, value) : juce::jmap(value, 0.0f, 100.0f); }
        else if (identifier == IDs::mod2GainAmt) { mod2GainAmt = !normed ? juce::jlimit(-12.0f, 12.0f, value) : juce::jmap(value, -12.0f, 12.0f); }
        else if (identifier == IDs::mod2PitchAmt) { mod2PitchAmt = !normed ? juce::jlimit(-12.0f, 12.0f, value) : juce::jmap(value, -12.0f, 12.0f); }
        else if (identifier == IDs::mod2PlayheadPos) { mod2PlayheadPos = !normed ? juce::jlimit(0.0f, 1.0f, value) : juce::jmap(value, 0.0f, 1.0f); }
        else if (identifier == IDs::vel2CutoffAmt) { vel2CutoffAmt = !normed ? juce::jlimit(0.0f, 100.0f, value) : juce::jmap(value, 0.0f, 100.0f); }
        else if (identifier == IDs::vel2GainAmt) { vel2GainAmt = !normed ? juce::jlimit(0.0f, 1.0f, value) : juce::jmap(value, 0.0f, 1.0f); }
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

void SourceSound::setParameterByNameInt(juce::Identifier identifier, int value){
    // --> Start auto-generated code D
        if (identifier == IDs::launchMode) { launchMode = juce::jlimit(0, 4, value); }
        else if (identifier == IDs::loopXFadeNSamples) { loopXFadeNSamples = juce::jlimit(10, 100000, value); }
        else if (identifier == IDs::reverse) { reverse = juce::jlimit(0, 1, value); }
        else if (identifier == IDs::noteMappingMode) { noteMappingMode = juce::jlimit(0, 3, value); }
        else if (identifier == IDs::numSlices) { numSlices = juce::jlimit(0, 100, value); }
        else if (identifier == IDs::midiChannel) { midiChannel = juce::jlimit(0, 16, value); }
        // --> End auto-generated code D
    else { throw std::runtime_error("No int parameter with this name"); }
}

juce::BigInteger SourceSound::getMappedMidiNotes(){
    // Returns the MIDI notes of the Sound (note that individual SourceSamplerSound(s) might have different assingments depending on root note)
    juce::BigInteger midiNotes;
    midiNotes.parseString(midiNotesAsString.get(), 16);
    return midiNotes;
}

int SourceSound::getNumberOfMappedMidiNotes(){
    // Get the mapped midi notes of the first sound
    return getMappedMidiNotes().countNumberOfSetBits();
}

void SourceSound::setMappedMidiNotes(juce::BigInteger newMappedMidiNotes){
    // Save the new midi notes in the Sound state
    midiNotesAsString = newMappedMidiNotes.toString(16);
    
    // Recalculate all assigned midi notes for the different SourceSamplerSound based on their root note
    assignMidiNotesAndVelocityToSourceSamplerSounds();
}

int closest(std::vector<int> const& vec, int value) {
    int minDistancePosition = 0;
    int minDistance = 9999;
    for (int i=0; i<vec.size(); i++){
        int distance = std::abs(vec[i] - value);
        if (distance < minDistance){
            minDistancePosition = i;
            minDistance = distance;
        }
    }
    return vec[minDistancePosition];
}

void SourceSound::assignMidiNotesAndVelocityToSourceSamplerSounds(){
    // 1) Assign each of the midi notes of the SourceSound to the closest root note from linked SourceSamplerSound(s)
    
    // Collect all available root notes
    std::vector<int> sourceSamplerSoundRootNotes = {};
    for (auto sourceSamplerSound: getLinkedSourceSamplerSounds()){
        sourceSamplerSoundRootNotes.push_back(sourceSamplerSound->getMidiRootNote());
    }
    
    // Iterate SourceSamplerSound(s) and assign midiNotes closer to their root note (only from those notes already assigned to SourceSound)
    juce::BigInteger sourceSoundMidiNotes = getMappedMidiNotes();
    for (auto sourceSamplerSound: getLinkedSourceSamplerSounds()){
        juce::BigInteger midiNotesForSourceSamplerSound = 0;
        for (int i=0; i<128; i++){
            if (sourceSoundMidiNotes[i] == true){
                if (closest(sourceSamplerSoundRootNotes, i) == sourceSamplerSound->getMidiRootNote()){
                    midiNotesForSourceSamplerSound.setBit(i);
                }
            }
        }
        sourceSamplerSound->setMappedMidiNotes(midiNotesForSourceSamplerSound);
    }
    
    // 2) Assign midi velocities
    
    // For each different root note, see how many velocity layers there are
    for (auto rootNote: sourceSamplerSoundRootNotes){
        std::vector<int> rootNoteVelocityLayers = {};  // Will store all distinct velocity layers for every root note
        for (auto sourceSamplerSound: getLinkedSourceSamplerSounds()){
            if (sourceSamplerSound->getMidiRootNote() == rootNote){
                int sourceSamplerSoundVelocityLayer = sourceSamplerSound->getMidiVelocityLayer();
                if (std::find(rootNoteVelocityLayers.begin(), rootNoteVelocityLayers.end(), sourceSamplerSoundVelocityLayer) == rootNoteVelocityLayers.end()) {
                    // If SourceSamplerSound's velocity layer not yet added to velocityLayers, add it now
                    rootNoteVelocityLayers.push_back(sourceSamplerSoundVelocityLayer);
                }
            }
        }
        if (rootNoteVelocityLayers.size() == 1){
            // If there's only one velocity layer, then the SourceSamplerSound(s) for that root note respond to all velocities (there should be only one)
            juce::BigInteger midiVelocities = 0;
            midiVelocities.setRange(0, 128, true);
            for (auto sourceSamplerSound: getLinkedSourceSamplerSounds()){
                if (sourceSamplerSound->getMidiRootNote() == rootNote){
                    sourceSamplerSound->setMappedVelocities(midiVelocities);
                }
            }
        } else {
            // If there are different velocity layers, then compute different midiVelocities for every SourceSamplerSound/rootNote and assign
            // velocity layers should be integer numbers starting from 0 and up to the number of available layers
            std::sort(rootNoteVelocityLayers.begin(), rootNoteVelocityLayers.end());
            int numLayers = (int)rootNoteVelocityLayers.size();
            int numVelocitiesPerLayer = 128 / numLayers;
            for (int i=0; i<numLayers; i++){
                juce::BigInteger midiVelocities = 0;
                int bitRangeStart = i * numVelocitiesPerLayer;
                int bitRangeEnd = juce::jmin(127, (i + 1) * numVelocitiesPerLayer);
                midiVelocities.setRange(bitRangeStart, bitRangeEnd, true);
                for (auto sourceSamplerSound: getLinkedSourceSamplerSounds()){
                    if ((sourceSamplerSound->getMidiRootNote() == rootNote) && (sourceSamplerSound->getMidiVelocityLayer() == rootNoteVelocityLayers[i])){
                        // Assign the computed midi velocities to SourceSamplerSound(s) with that root note and velocity layer (there should be only one)
                        sourceSamplerSound->setMappedVelocities(midiVelocities);
                    }
                }
            }
        }
    }
}

void SourceSound::setMidiRootNote(int newMidiRootNote){
    // Set the midi root note to sounds' SourceSamplerSound
    if (getLinkedSourceSamplerSounds().size() > 1){
        // If the sound has more than one SourceSamplerSound, no root note is updated as we don't know which sound should be updated
    } else {
        getFirstLinkedSourceSamplerSound()->setMidiRootNote(newMidiRootNote);
    }
}


// ------------------------------------------------------------------------------------------------

void SourceSound::setOnsetTimesSamples(std::vector<float> onsetTimes){
    // Set the given onset times as slices
    // NOTE: this method should not be called for multi-sample sounds (should I assert here?)
    SourceSamplerSound* first = getFirstLinkedSourceSamplerSound();
    if (first != nullptr){
        first->setOnsetTimesSamples(onsetTimes);
    }
}

// ------------------------------------------------------------------------------------------------

void SourceSound::addOrEditMidiMapping(juce::String uuid, int ccNumber, juce::String parameterName, float minRange, float maxRange){
    
    juce::ValueTree existingMapping = state.getChildWithProperty(IDs::uuid, uuid);
    if (existingMapping.isValid()) {
        // Modify existing mapping
        existingMapping.setProperty (IDs::ccNumber, ccNumber, nullptr);
        existingMapping.setProperty (IDs::parameterName, parameterName, nullptr);
        existingMapping.setProperty (IDs::minRange, juce::jlimit(0.0f, 1.0f, minRange), nullptr);
        existingMapping.setProperty (IDs::maxRange, juce::jlimit(0.0f, 1.0f, maxRange), nullptr);
    } else {
        // No mapping with uuid found, create a new one
        // TODO: move this to the MidiCCMappingList class?
        juce::ValueTree newMapping = Helpers::createMidiMappingState(ccNumber, parameterName, juce::jlimit(0.0f, 1.0f, minRange), juce::jlimit(0.0f, 1.0f, maxRange));
        state.addChild(newMapping, -1, nullptr);
    }
}

std::vector<MidiCCMapping*> SourceSound::getMidiMappingsForCcNumber(int ccNumber){
    // TODO: transofrm to use pointer
    std::vector<MidiCCMapping*> toReturn = {};
    for (auto* midiCCmapping: midiCCmappings->objects){
        if (midiCCmapping->ccNumber == ccNumber){
            toReturn.push_back(midiCCmapping);
        }
    }
    return toReturn;
}

void SourceSound::removeMidiMapping(juce::String uuid){
    midiCCmappings->removeMidiCCMappingWithUUID(uuid);
}

// ------------------------------------------------------------------------------------------------

std::vector<SourceSamplerSound*> SourceSound::createSourceSamplerSounds ()
{
    // Generate all the SourceSamplerSound objects corresponding to this sound
    // In most of the cases this will be a single sound, but it could be that
    // some sounds have more than one SourceSamplerSound (multi-layered sounds
    // for example)
    
    juce::AudioFormatManager audioFormatManager;
    audioFormatManager.registerBasicFormats();
    std::vector<SourceSamplerSound*> sounds = {};
    for (int i=0; i<state.getNumChildren(); i++){
        auto child = state.getChild(i);
        if (child.hasType(IDs::SOUND_SAMPLE)){
            juce::File locationInDisk = getGlobalContext().sourceDataLocation.getChildFile(child.getProperty(IDs::filePath, "").toString());
            if (fileAlreadyInDisk(locationInDisk) && fileLocationIsSupportedAudioFileFormat(locationInDisk)){
                std::unique_ptr<juce::AudioFormatReader> reader(audioFormatManager.createReaderFor(locationInDisk));
                SourceSamplerSound* createdSound = new SourceSamplerSound(child,
                                                                          this,
                                                                          *reader,
                                                                          MAX_SAMPLE_LENGTH,
                                                                          getGlobalContext().sampleRate,
                                                                          getGlobalContext().samplesPerBlock);
                sounds.push_back(createdSound);
            }
        }
    }
    return sounds;
}

void SourceSound::addSourceSamplerSoundsToSampler()
{
    std::vector<SourceSamplerSound*> sourceSamplerSounds = createSourceSamplerSounds();
    for (auto sourceSamplerSound: sourceSamplerSounds) { getGlobalContext().sampler->addSound(sourceSamplerSound); }
    assignMidiNotesAndVelocityToSourceSamplerSounds();
    std::cout << "Added " << sourceSamplerSounds.size() << " SourceSamplerSound(s) to sampler... " << std::endl;
    allSoundsLoaded = true;
}

void SourceSound::removeSourceSampleSoundsFromSampler()
{
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

// ------------------------------------------------------------------------------------------------

void SourceSound::loadSounds()
{
    // Load all the  SourceSamplerSound(s) of SourceSound, triggering downloading if needed. This will normally be one single sound except for the
    // case of multi-sample sounds in which there might be different sounds per pitch and velocity layers.
    
    allSoundsLoaded = false;
    
    bool allAlreadyDownloaded = true;
    for (int i=0; i<state.getNumChildren(); i++){
        auto child = state.getChild(i);
        if (child.hasType(IDs::SOUND_SAMPLE)){
            
            bool isFromFreesound = child.getProperty(IDs::soundFromFreesound, false);
            juce::String filePath = child.getProperty(IDs::filePath, ""); // Relative file path
            juce::File locationInDisk; // The location in disk where sound should be downloaded/placed
            
            if (isFromFreesound){
                // Check if sound already exists in disk, otherwise trigger download
                // If sound is from Freesound, compute the expected file location and update the state with that location (it will most likely be the same)
                // This can be useful if sharing presets that had downloaded original quality files with plugin instances that don't have the ability to
                // download original quality files (e.g. have no access token set)
                locationInDisk = getFreesoundFileLocation(child);
                child.setProperty(IDs::filePath, locationInDisk.getRelativePathFrom(getGlobalContext().sourceDataLocation), nullptr);
                if (fileAlreadyInDisk(locationInDisk)){
                    // If file already exists at the expected location, mark it as downloaded and don't trigger downloading
                    child.setProperty(IDs::downloadProgress, 100.0, nullptr);
                    child.setProperty(IDs::downloadCompleted, true, nullptr);
                } else {
                    // If the sound does not exist, then trigger the download
                    allAlreadyDownloaded = false;
                    child.setProperty(IDs::downloadProgress, 0.0, nullptr);
                    child.setProperty(IDs::downloadCompleted, false, nullptr);
                    if (shouldUseOriginalQualityFile(child)){
                        // Download original quality file
                        juce::String downloadURL = juce::String("https://freesound.org/apiv2/sounds/<sound_id>/download/").replace("<sound_id>", child.getProperty(IDs::soundId).toString(), false);
                        # if !USE_EXTERNAL_HTTP_SERVER_FOR_DOWNLOADS
                        juce::URL::DownloadTaskOptions options = juce::URL::DownloadTaskOptions().withExtraHeaders("Authorization: Bearer " + getGlobalContext().freesoundOauthAccessToken).withListener(this);
                        std::unique_ptr<juce::URL::DownloadTask> downloadTask = juce::URL(downloadURL).downloadToFile(locationInDisk, options);
                        downloadTasks.push_back(std::move(downloadTask));
                        DBG("Downloading sound to " << locationInDisk.getFullPathName());
                        # else
                        juce::URL downloadServerUrlEndpoint;
                        downloadServerUrlEndpoint = juce::URL("http://localhost:" + String(HTTP_DOWNLOAD_SERVER_PORT) + "/download_sound").withParameter("soundUUID", getUUID()).withParameter("urlToDownloadFrom", downloadURL).withParameter("pathToSaveDownloadedFile", locationInDisk.getFullPathName() ).withParameter("downloadHeaders", "Authorization: Bearer " + getGlobalContext().freesoundOauthAccessToken);
                        int statusCode = -1;
                        StringPairArray responseHeaders;
                        if (auto stream = std::unique_ptr<InputStream>(downloadServerUrlEndpoint.createInputStream(false, nullptr, nullptr, "",
                            MAX_DOWNLOAD_WAITING_TIME_MS, // timeout in millisecs
                            &responseHeaders, &statusCode))){
                            String resp = stream->readEntireStreamAsString();
                            DBG("Downloading sound to " << locationInDisk.getFullPathName() << " with external server: " + (String)statusCode + ": " + resp);
                        } else {
                            DBG("Download server ERROR downloading sound to " << locationInDisk.getFullPathName());
                        }
                        # endif
                    } else {
                        // Download preview quality file
                        child.setProperty(IDs::usesPreview, true, nullptr);
                        juce::String previewURL = child.getProperty(IDs::previewURL, "").toString();
                        # if !USE_EXTERNAL_HTTP_SERVER_FOR_DOWNLOADS
                        juce::URL::DownloadTaskOptions options = juce::URL::DownloadTaskOptions().withListener(this);
                        std::unique_ptr<juce::URL::DownloadTask> downloadTask = juce::URL(previewURL).downloadToFile(locationInDisk, options);
                        downloadTasks.push_back(std::move(downloadTask));
                        DBG("Downloading sound to " << locationInDisk.getFullPathName());
                        # else
                        juce::URL downloadServerUrlEndpoint;
                        downloadServerUrlEndpoint = juce::URL("http://localhost:" + String(HTTP_DOWNLOAD_SERVER_PORT) + "/download_sound").withParameter("soundUUID", getUUID()).withParameter("urlToDownloadFrom", previewURL).withParameter("pathToSaveDownloadedFile", locationInDisk.getFullPathName() ).withParameter("downloadHeaders", "");
                        int statusCode = -1;
                        StringPairArray responseHeaders;
                        if (auto stream = std::unique_ptr<InputStream>(downloadServerUrlEndpoint.createInputStream(false, nullptr, nullptr, "",
                            MAX_DOWNLOAD_WAITING_TIME_MS, // timeout in millisecs
                            &responseHeaders, &statusCode))){
                            String resp = stream->readEntireStreamAsString();
                            DBG("Downloading sound to " << locationInDisk.getFullPathName() << " with external server: " + (String)statusCode + ": " + resp);
                        } else {
                            DBG("Download server ERROR downloading sound to " << locationInDisk.getFullPathName());
                        }
                        # endif
                    }
                }
            } else {
                // Chek if sound already exists in disk, otherwise there's nothing we can do as the sound is not from Freesound we can't re-download it
                if (filePath != ""){
                    locationInDisk = getGlobalContext().sourceDataLocation.getChildFile(filePath);
                    if (fileAlreadyInDisk(locationInDisk)){
                        child.setProperty(IDs::downloadProgress, 100.0, nullptr);
                        child.setProperty(IDs::downloadCompleted, true, nullptr);
                    }
                }
            }
        }
    }
    
    if (allAlreadyDownloaded){
        // If no sound needs downloading
        addSourceSamplerSoundsToSampler();
    }
}

bool SourceSound::isSupportedAudioFileFormat(const juce::String& extension)
{
    return juce::StringArray({"ogg", "wav", "aiff", "mp3", "flac"}).contains(extension.toLowerCase().replace(".", ""));
}


bool SourceSound::fileLocationIsSupportedAudioFileFormat(juce::File location)
{
    return isSupportedAudioFileFormat(location.getFileExtension());
}

juce::File SourceSound::getFreesoundFileLocation(juce::ValueTree sourceSamplerSoundState)
{
    juce::File locationInDisk;
    juce::String soundId = sourceSamplerSoundState.getProperty(IDs::soundId);
    if (shouldUseOriginalQualityFile(sourceSamplerSoundState)){
        locationInDisk = getGlobalContext().soundsDownloadLocation.getChildFile(soundId + "-original").withFileExtension(sourceSamplerSoundState.getProperty(IDs::format).toString());
    } else {
        locationInDisk = getGlobalContext().soundsDownloadLocation.getChildFile((juce::String)soundId).withFileExtension("ogg");
    }
    return locationInDisk;
}

bool SourceSound::shouldUseOriginalQualityFile(juce::ValueTree sourceSamplerSoundState)
{
    if (getGlobalContext().freesoundOauthAccessToken != ""){
        if (getGlobalContext().useOriginalFilesPreference == USE_ORIGINAL_FILES_ALWAYS){
            return true;
        } else if (getGlobalContext().useOriginalFilesPreference == USE_ORIGINAL_FILES_ONLY_SHORT){
            // Only return true if sound has a filesize below the threshold
            if ((int)sourceSamplerSoundState.getProperty(IDs::filesize) <= MAX_SIZE_FOR_ORIGINAL_FILE_DOWNLOAD){
                return true;
            }
        }
    }
    return false;
}

bool SourceSound::fileAlreadyInDisk(juce::File locationInDisk)
{
    // Check that file exists in disk and that it has at least half of the expected full size
    int fileMinSize = 100; // Something smaller than 100 bytes will be considered corrupt file
    return locationInDisk.existsAsFile() && locationInDisk.getSize() > fileMinSize;
}

void SourceSound::downloadProgressUpdate(juce::File targetFileLocation, float percentageCompleted)
{
    for (int i=0; i<state.getNumChildren(); i++){
        auto child = state.getChild(i);
        if (child.hasType(IDs::SOUND_SAMPLE)){
            juce::File locationInDisk = getGlobalContext().sourceDataLocation.getChildFile(child.getProperty(IDs::filePath, "").toString());
            if (targetFileLocation == locationInDisk){
                // Find the sample that corresponds to this download task and update state
                child.setProperty(IDs::downloadProgress, percentageCompleted, nullptr);
            }
        }
    }
}

void SourceSound::downloadFinished(juce::File targetFileLocation, bool taskSucceeded)
{
    bool allAlreadyDownloaded = true;
    for (int i=0; i<state.getNumChildren(); i++){
        auto child = state.getChild(i);
        if (child.hasType(IDs::SOUND_SAMPLE)){
            // Find the sample that corresponds to this download task and update state
            // To consider a download as succeeded, check that the the task returned succes
            juce::File locationInDisk = getGlobalContext().sourceDataLocation.getChildFile(child.getProperty(IDs::filePath, "").toString());
            if (taskSucceeded && (targetFileLocation == locationInDisk) && fileAlreadyInDisk(locationInDisk)){
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

void SourceSound::progress (juce::URL::DownloadTask *task, juce::int64 bytesDownloaded, juce::int64 totalLength)
{
    float percentageCompleted = 100.0*(float)bytesDownloaded/(float)totalLength;
    downloadProgressUpdate(task->getTargetLocation(), percentageCompleted);
}

void SourceSound::finished(juce::URL::DownloadTask *task, bool success)
{
    downloadFinished(task->getTargetLocation(), success);
}
