/*
  ==============================================================================

    SourceSamplerSound.cpp
    Created: 23 Mar 2022 1:38:02pm
    Author:  Frederic Font Corbera

  ==============================================================================
*/

#include "SourceSamplerSound.h"


SourceSamplerSound::SourceSamplerSound (const juce::ValueTree& _state,
                                        SourceSound* _sourceSoundPointer,
                                        AudioFormatReader& source,
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
        lengthInSamples = jmin ((int) source.lengthInSamples, (int) (maxSampleLengthSeconds * soundSampleRate));
        data.reset (new AudioBuffer<float> (jmin (2, (int) source.numChannels), lengthInSamples + 4));
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
    name.referTo(state, IDs::name, nullptr);
    soundId.referTo(state, IDs::soundId, nullptr);
    midiRootNote.referTo(state, IDs::midiRootNote, nullptr);
    midiNotesAsString.referTo(state, IDs::midiNotes, nullptr);
}

void SourceSamplerSound::writeBufferToDisk()
{
    juce::AudioBuffer<float>* buffer = getAudioData();
    juce::WavAudioFormat format;
    std::unique_ptr<AudioFormatWriter> writer;
    
    juce::File outputLocation = File::getSpecialLocation(File::userDocumentsDirectory).getChildFile("SourceSampler/tmp/" + state.getProperty(IDs::uuid).toString()).withFileExtension("wav");
    writer.reset (format.createWriterFor (new FileOutputStream (outputLocation),
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
    return getMappedMidiNotes()[midiNoteNumber];
}

bool SourceSamplerSound::appliesToChannel (int /*midiChannel*/)
{
    return true;
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
    juce::BigInteger midiNotes;
    midiNotes.parseString(midiNotesAsString.get(), 16);
    return midiNotes;
}

void SourceSamplerSound::setMappedMidiNotes(juce::BigInteger newMappedMidiNotes)
{
    midiNotesAsString = newMappedMidiNotes.toString(16);
}

int SourceSamplerSound::getMidiRootNote()
{
    return midiRootNote;
}

void SourceSamplerSound::setMidiRootNote(int newRootNote)
{
    midiRootNote = newRootNote;
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
    ValueTree soundAnalysis = state.getChildWithName(IDs::ANALYSIS);
    if (soundAnalysis.isValid()){
        ValueTree onsetTimes = soundAnalysis.getChildWithName(IDs::onsets);
        if (onsetTimes.isValid()){
            std::vector<float> onsets = {};
            for (int i=0; i<onsetTimes.getNumChildren(); i++){
                ValueTree onsetVT = onsetTimes.getChild(i);
                float onset = (float)(onsetVT.getProperty(IDs::onsetTime));
                onsets.push_back(onset);
            }
            setOnsetTimesSamples(onsets);
        }
    }
}


//==============================================================================


SourceSound::SourceSound (const juce::ValueTree& _state,
             std::function<GlobalContextStruct()> globalContextGetter): Thread ("LoaderThread"), state(_state)
{
    getGlobalContext = globalContextGetter;
    bindState();
    triggerSoundDownloads();
}

SourceSound::~SourceSound ()
{
    for (int i = 0; i < downloadTasks.size(); i++) {
        downloadTasks.at(i).reset();
    }
}

void SourceSound::bindState ()
{
    name.referTo(state, IDs::name, nullptr);
    enabled.referTo(state, IDs::enabled, nullptr);
    allSoundsLoaded.referTo(state, IDs::allSoundsLoaded, nullptr);
    
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

juce::String SourceSound::getName() {
    return name.get();
}

juce::String SourceSound::getUUID() {
    return state.getProperty(IDs::uuid).toString();
}

bool SourceSound::isEnabled() {
    return enabled.get();
}

int SourceSound::getParameterInt(juce::Identifier identifier){
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

float SourceSound::getParameterFloat(juce::Identifier identifier, bool normed){
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

void SourceSound::setParameterByNameFloat(juce::Identifier identifier, float value, bool normed){
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

void SourceSound::setParameterByNameInt(juce::Identifier identifier, int value){
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

juce::BigInteger SourceSound::getMappedMidiNotes(){
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

int SourceSound::getNumberOfMappedMidiNotes(){
    // Get the mapped midi notes of the first sound
    // NOTE: this method should not be called for multi-sample sounds (should I assert here?)
    return getMappedMidiNotes().countNumberOfSetBits();
}

void SourceSound::setMappedMidiNotes(juce::BigInteger newMappedMidiNotes){
    // Set the mapped midi notes to all of the sampler sounds
    // NOTE: this method should not be called for multi-sample sounds (should I assert here?)
    for (auto sourceSamplerSound: getLinkedSourceSamplerSounds()){
        sourceSamplerSound->setMappedMidiNotes(newMappedMidiNotes);
    }
}

int SourceSound::getMidiRootNote(){
    // Get the midi root note of the first sound
    // NOTE: this method should not be called for multi-sample sounds (should I assert here?)
    SourceSamplerSound* first = getFirstLinkedSourceSamplerSound();
    if (first != nullptr){
        return first->getMidiRootNote();
    }
    return -1;
}

void SourceSound::setMidiRootNote(int newMidiRootNote){
    // Set the midi root note to all of the sampler sounds
    // NOTE: this method should not be called for multi-sample sounds (should I assert here?)
    for (auto sourceSamplerSound: getLinkedSourceSamplerSounds()){
        sourceSamplerSound->setMidiRootNote(newMidiRootNote);
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

void SourceSound::addOrEditMidiMapping(juce::String uuid, int ccNumber, String parameterName, float minRange, float maxRange){
    
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
    
    AudioFormatManager audioFormatManager;
    audioFormatManager.registerBasicFormats();
    std::vector<SourceSamplerSound*> sounds = {};
    for (int i=0; i<state.getNumChildren(); i++){
        auto child = state.getChild(i);
        if (child.hasType(IDs::SOUND_SAMPLE)){
            File locationInDisk = getGlobalContext().sourceDataLocation.getChildFile(child.getProperty(IDs::filePath, "").toString());
            if (fileAlreadyInDisk(locationInDisk) && fileLocationIsSupportedAudioFileFormat(locationInDisk)){
                std::unique_ptr<AudioFormatReader> reader(audioFormatManager.createReaderFor(locationInDisk));
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

bool SourceSound::isSupportedAudioFileFormat(const String& extension)
{
    return StringArray({"ogg", "wav", "aiff", "mp3", "flac"}).contains(extension.toLowerCase().replace(".", ""));
}


bool SourceSound::fileLocationIsSupportedAudioFileFormat(File location)
{
    return isSupportedAudioFileFormat(location.getFileExtension());
}

File SourceSound::getFreesoundFileLocation(juce::ValueTree sourceSamplerSoundState)
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

bool SourceSound::fileAlreadyInDisk(File locationInDisk)
{
    // Check that file exists in disk and that it has at least half of the expected full size
    int fileMinSize = 100; // Something smaller than 100 bytes will be considered corrupt file
    return locationInDisk.existsAsFile() && locationInDisk.getSize() > fileMinSize;
}

void SourceSound::triggerSoundDownloads()
{
    // Trigger the downloading of all SourceSamplerSound(s) of SourceSound. This will normally be one single sound except for the
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
                        URL::DownloadTaskOptions options = URL::DownloadTaskOptions().withExtraHeaders("Authorization: Bearer " + getGlobalContext().freesoundOauthAccessToken).withListener(this);
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
                        URL::DownloadTaskOptions options = URL::DownloadTaskOptions().withListener(this);
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

void SourceSound::downloadProgressUpdate(File targetFileLocation, float percentageCompleted)
{
    for (int i=0; i<state.getNumChildren(); i++){
        auto child = state.getChild(i);
        if (child.hasType(IDs::SOUND_SAMPLE)){
            File locationInDisk = getGlobalContext().sourceDataLocation.getChildFile(child.getProperty(IDs::filePath, "").toString());
            if (targetFileLocation == locationInDisk){
                // Find the sample that corresponds to this download task and update state
                child.setProperty(IDs::downloadProgress, percentageCompleted, nullptr);
            }
        }
    }
}

void SourceSound::downloadFinished(File targetFileLocation, bool taskSucceeded)
{
    bool allAlreadyDownloaded = true;
    for (int i=0; i<state.getNumChildren(); i++){
        auto child = state.getChild(i);
        if (child.hasType(IDs::SOUND_SAMPLE)){
            // Find the sample that corresponds to this download task and update state
            // To consider a download as succeeded, check that the the task returned succes
            File locationInDisk = getGlobalContext().sourceDataLocation.getChildFile(child.getProperty(IDs::filePath, "").toString());
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

void SourceSound::progress (URL::DownloadTask *task, int64 bytesDownloaded, int64 totalLength)
{
    float percentageCompleted = 100.0*(float)bytesDownloaded/(float)totalLength;
    downloadProgressUpdate(task->getTargetLocation(), percentageCompleted);
}

void SourceSound::finished(URL::DownloadTask *task, bool success)
{
    downloadFinished(task->getTargetLocation(), success);
}
 
void SourceSound::run(){
    // TODO: use thread to download and load sounds (?)
}
