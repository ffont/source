/*
  ==============================================================================

    SourceSamplerSound.cpp
    Created: 23 Mar 2022 1:38:02pm
    Author:  Frederic Font Corbera

  ==============================================================================
*/

#include "SourceSamplerSound.h"
#include "SourceSamplerVoice.h"
#include "SourceSamplerSynthesiser.h"
#include "SourceParameters.h"


SourceSamplerSound::SourceSamplerSound (const juce::ValueTree& _state,
                                        SourceSound* _sourceSoundPointer,
                                        std::unique_ptr<juce::AudioBuffer<float>> audioData,
                                        double _soundSampleRate,
                                        double _pluginSampleRate,
                                        int _pluginBlockSize)
    : stretchProcessorThread (*this),
      state(_state),
      soundSampleRate (_soundSampleRate),
      pluginSampleRate (_pluginSampleRate),
      pluginBlockSize (_pluginBlockSize)
{
    jassert (juce::MessageManager::getInstanceWithoutCreating() == nullptr || juce::MessageManager::getInstance()->isThisTheMessageThread());
    
    sourceSoundPointer = _sourceSoundPointer;
    bindState();
    
    if (audioData == nullptr){
        audioData = std::make_unique<juce::AudioBuffer<float>>(1, 1);
        audioData->clear();
    }
    
    // Playback data is initially the original data (no stretching)
    data = std::shared_ptr<const juce::AudioBuffer<float>> (std::move (audioData));
    playbackData = data;
    lengthInSamples = data->getNumSamples();
    
    // Add duration to state
    state.setProperty(SourceIDs::duration, getLengthInSeconds(), nullptr);
    
    // Load calculated onsets (if any)
    loadOnsetTimesSamplesFromAnalysis();
    
    // Schedule pre-processing of audio data with stretch (so time stretching/pitch shifting is not computed in real time)
    // This does nothing if no stretching is needed
    setStretchParameters(getParameterFloat(SourceIDs::pitchShift), getParameterFloat(SourceIDs::timeStretch));
}

SourceSamplerSound::~SourceSamplerSound()
{
    stopTimer();
    stretchProcessorThread.stopThread(2000);
}

void SourceSamplerSound::bindState ()
{
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::name, SourceDefaults::name);
    name.referTo(state, SourceIDs::name, nullptr);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::soundId, SourceDefaults::soundId);
    soundId.referTo(state, SourceIDs::soundId, nullptr);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::midiRootNote, SourceDefaults::midiRootNote);
    midiRootNote.referTo(state, SourceIDs::midiRootNote, nullptr);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::midiVelocityLayer, SourceDefaults::midiVelocityLayer);
    midiVelocityLayer.referTo(state, SourceIDs::midiVelocityLayer, nullptr);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::sampleStartPosition, SourceDefaults::sampleStartPosition);
    sampleStartPosition.referTo(state, SourceIDs::sampleStartPosition, nullptr);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::sampleEndPosition, SourceDefaults::sampleEndPosition);
    sampleEndPosition.referTo(state, SourceIDs::sampleEndPosition, nullptr);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::sampleLoopStartPosition, SourceDefaults::sampleLoopStartPosition);
    sampleLoopStartPosition.referTo(state, SourceIDs::sampleLoopStartPosition, nullptr);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::sampleLoopEndPosition, SourceDefaults::sampleLoopEndPosition);
    sampleLoopEndPosition.referTo(state, SourceIDs::sampleLoopEndPosition, nullptr);
    checkSampleSampleStartEndAndLoopPositions();
}

void SourceSamplerSound::timerCallback()
{
    // This timer only runs while there is a pending stretch processing job
    if (shouldProcessWithStretchAtTime < 0){
        stopTimer();
        return;
    }
    if ((juce::Time::getMillisecondCounterHiRes() > shouldProcessWithStretchAtTime) && !stretchProcessorThread.isThreadRunning()){
        shouldProcessWithStretchAtTime = -1.0;
        stopTimer();
        DBG("Pre-processing with stretch...");
        stretchProcessorThread.startThread(); // Runs preProcessAudioWithStretch() in thread
    }
}

void SourceSamplerSound::writeWaveformFileToDisk (const juce::AudioBuffer<float>& buffer, double sampleRate, const juce::File& outputLocation)
{
    if (buffer.getNumSamples() == 0 || sampleRate <= 0)
        return;
    
    // Write files with lower resolution as it will be enough for waveform visualization: mono, 8 bits, 16kHz
    const double outputSampleRate = 16000.0;
    const double ratio = sampleRate / outputSampleRate;
    const int numOutputSamples = juce::jmax (1, (int) (buffer.getNumSamples() / ratio));
    juce::AudioBuffer<float> downsampled (1, numOutputSamples);
    const float* in = buffer.getReadPointer (0);
    float* out = downsampled.getWritePointer (0);
    for (int i = 0; i < numOutputSamples; ++i)
        out[i] = in[juce::jmin (buffer.getNumSamples() - 1, (int) (i * ratio))];
    
    outputLocation.deleteFile();
    juce::WavAudioFormat format;
    std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (outputLocation);
    auto options = juce::AudioFormatWriterOptions{}.withSampleRate (outputSampleRate)
                                                   .withNumChannels (1)
                                                   .withBitsPerSample (8);
    if (auto writer = format.createWriterFor (stream, options))
        writer->writeFromAudioSampleBuffer (downsampled, 0, downsampled.getNumSamples());
}

void SourceSamplerSound::preProcessAudioWithStretch()
{
    // Runs in the stretch processor thread. Computes the new buffer without touching the one being used for playback,
    // and then swaps buffers under the playback lock (which voices hold while rendering).
    computingTimeStretch = true;
    const float timeStretchRatio = nextTimeStretchRatio.load();
    const float pitchShiftSemitones = nextPitchShiftSemitones.load();
    
    std::shared_ptr<const juce::AudioBuffer<float>> newPlaybackData;
    if ((timeStretchRatio != 1.0f) || (pitchShiftSemitones != 0.0f)){
        // Compute stretched version of audio into a new buffer
        const int newStretchedBufferNumSamples = juce::jmax (1, int(timeStretchRatio * data->getNumSamples()));
        auto stretched = std::make_shared<juce::AudioBuffer<float>>(data->getNumChannels(), newStretchedBufferNumSamples);
        stretch.configure(data->getNumChannels(), (int)(soundSampleRate * 0.1), (int)(soundSampleRate * 0.04));
        stretch.reset();
        stretch.setTransposeSemitones(pitchShiftSemitones);
        stretch.process(data->getArrayOfReadPointers(), data->getNumSamples(), stretched->getArrayOfWritePointers(), stretched->getNumSamples());
        newPlaybackData = stretched;
    } else {
        // If the stretching parameters are to leave the sound as it is, then we just use the original audio data
        newPlaybackData = data;
    }
    
    {
        const juce::SpinLock::ScopedLockType sl (playbackDataLock);
        std::swap (playbackData, newPlaybackData);
        lengthInSamples = playbackData->getNumSamples();
    }
    // The old buffer (now in newPlaybackData) gets released here, outside the lock and outside the audio thread
    newPlaybackData.reset();
    computingTimeStretch = false;
}

void SourceSamplerSound::setStretchParameters(float newPitchShiftSemitones, float newTimeStretchRatio) {
    newTimeStretchRatio = juce::jlimit (0.1f, maxTimeStretchRatio, newTimeStretchRatio);
    if (newTimeStretchRatio == appliedTimeStretchRatio && newPitchShiftSemitones == appliedPitchShiftSemitones)
        return;  // Nothing to do
    appliedTimeStretchRatio = newTimeStretchRatio;
    appliedPitchShiftSemitones = newPitchShiftSemitones;
    nextTimeStretchRatio = newTimeStretchRatio;
    nextPitchShiftSemitones = newPitchShiftSemitones;
    // Debounce processing so we don't re-compute stretch on every slider movement
    shouldProcessWithStretchAtTime = juce::Time::getMillisecondCounterHiRes() + STRETCH_PROCESSING_TIME_DEBOUNCE_MS;
    if (!isTimerRunning())
        startTimer(SAMPLER_SOUND_TIMER_MS);
}

bool SourceSamplerSound::appliesToNote (int midiNoteNumber)
{
    // If sound is disabled, return false so new notes are not triggered
    return !sourceSoundPointer->isScheduledForDeletion() && getMappedMidiNotes()[midiNoteNumber];
}

bool SourceSamplerSound::appliesToVelocity (int midiVelocity)
{
    return midiVelocities[getCorrectedVelocity(midiVelocity)];
}

int SourceSamplerSound::getCorrectedVelocity(int midiVelocity)
{
    // Modified velocity value according to velSensitivity parameter (midiVelocity 0-127)
    return (int)std::round(127.0 * juce::jlimit(0.0, 1.0, std::pow((double)midiVelocity/127.0, getParameterFloat(SourceIDs::velSensitivity))));
}

float SourceSamplerSound::getCorrectedVelocity(float midiVelocity)
{
    // Modified velocity value according to velSensitivity parameter (midiVelocity 0.0-1.0)
    return (float)juce::jlimit(0.0, 1.0, std::pow((double)midiVelocity, getParameterFloat(SourceIDs::velSensitivity)));
}

bool SourceSamplerSound::appliesToChannel (int midiChannel)
{
    auto* mpeSynth = static_cast<SourceSamplerSynthesiser*>(sourceSoundPointer->getSampler());
    if (mpeSynth != nullptr && mpeSynth->isMpeEnabled()){
        // In MPE mode channels identify notes (not sounds), so all sounds respond to all channels
        juce::ignoreUnused(midiChannel);
        return true;
    }
    int soundMidiChannel = getParameterInt(SourceIDs::midiChannel);
    if (soundMidiChannel == 0){
        // use global channel (read from an atomic in the sampler: this is called from the audio thread)
        auto* synth = static_cast<SourceSamplerSynthesiser*>(sourceSoundPointer->getSampler());
        int globalMidiChannel = synth != nullptr ? synth->getGlobalMidiInChannel() : 0;
        return globalMidiChannel == 0 || globalMidiChannel == midiChannel;
    } else {
        // use the sound channel
        return soundMidiChannel == midiChannel;
    }
}

float SourceSamplerSound::getParameterFloat(juce::Identifier identifier){
    // Return parameters from the corresponding SourceSound object
    // For some parameters, first check if the SourceSamplerSound object has a special "override"
    if ((identifier == SourceIDs::startPosition) && (sampleStartPosition >= 0.0)){
        return sampleStartPosition;
    } else if ((identifier == SourceIDs::endPosition) && (sampleEndPosition >= 0.0)){
        return sampleEndPosition;
    } else if ((identifier == SourceIDs::loopStartPosition) && (sampleLoopStartPosition >= 0.0)){
        return sampleLoopStartPosition;
    } else if ((identifier == SourceIDs::loopEndPosition) && (sampleLoopEndPosition >= 0.0)){
        return sampleLoopEndPosition;
    }
    return sourceSoundPointer->getParameterFloat(identifier, false);
}

int SourceSamplerSound::getParameterInt(juce::Identifier identifier){
    return sourceSoundPointer->getParameterInt(identifier);
}

void SourceSamplerSound::setSampleStartEndAndLoopPositions(float start, float end, float loopStart, float loopEnd){
    sampleStartPosition = start;
    sampleEndPosition = end;
    sampleLoopStartPosition = loopStart;
    sampleLoopEndPosition = loopEnd;
    checkSampleSampleStartEndAndLoopPositions();
}

void SourceSamplerSound::checkSampleSampleStartEndAndLoopPositions(){
    // Check that start/end/loop times are "in order". Because these parameters are optional and can be set to -1.0 if the global
    // SourceSound value should be taken instead of the "local" sample value, we can not ensure 100% that parameters will always be
    // "in order" as we should compare also with SourceSound's values. Doing that would complicate the logic too much so we decided
    // not to do it. In worst case scenario, badly configured sample start/end/loop times will result in sound not being audible (which
    // also kind of makes sense)
    if ((sampleEndPosition >= 0.0) && (sampleStartPosition >= 0.0) && (sampleEndPosition < sampleStartPosition)) {
        sampleEndPosition = sampleStartPosition.get();
    }
    if ((sampleLoopStartPosition >= 0.0) && (sampleStartPosition >= 0.0) && (sampleLoopStartPosition < sampleStartPosition)){
        sampleLoopStartPosition = sampleStartPosition.get();
    }
    if ((sampleLoopEndPosition >= 0.0) && (sampleEndPosition >= 0.0) && (sampleLoopEndPosition > sampleEndPosition)){
        sampleLoopEndPosition = sampleEndPosition.get();
    }
    if ((sampleLoopStartPosition >= 0.0) && (sampleLoopEndPosition >= 0.0) && (sampleLoopStartPosition > sampleLoopEndPosition)){
        sampleLoopStartPosition = sampleLoopEndPosition.get();
    }
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



void SourceSamplerSound::loadOnsetTimesSamplesFromAnalysis(){
    juce::ValueTree soundAnalysis = state.getChildWithName(SourceIDs::ANALYSIS);
    if (soundAnalysis.isValid()){
        juce::ValueTree onsetTimes = soundAnalysis.getChildWithName(SourceIDs::onsets);
        if (onsetTimes.isValid()){
            std::vector<float> onsets = {};
            for (int i=0; i<onsetTimes.getNumChildren(); i++){
                juce::ValueTree onsetVT = onsetTimes.getChild(i);
                float onset = (float)(onsetVT.getProperty(SourceIDs::onsetTime));
                onsets.push_back(onset);
            }
            setOnsetTimesSamples(onsets);
        }
    }
}

// --------------------------------------------------------------------------------------------

bool SourceSamplerSound::isScheduledForDeletion() {
    return willBeDeleted;
}

void SourceSamplerSound::scheduleSampleSoundDeletion(){
    // Note that the deletion mechanism here is a bit simpler compared to that of SourceSound. This is because the stopping of
    // all the voices currently playing shat SourceSamplerSound will be done in the SourceSound method that is used to shcedule the
    // deletion of a SourceSampleSound (and that will in its turn call this method)
    willBeDeleted = true;
    scheduledForDeletionTime = juce::Time::getMillisecondCounterHiRes();
}

bool SourceSamplerSound::shouldBeDeleted(){
    return isScheduledForDeletion() && ((juce::Time::getMillisecondCounterHiRes() - scheduledForDeletionTime) > SAFE_SOUND_DELETION_TIME_MS);
}


//==============================================================================


SourceSound::SourceSound (const juce::ValueTree& _state,
             std::function<GlobalContextStruct()> globalContextGetter): state(_state), soundLoaderThread (*this)
{
    weakThis = this;
    getGlobalContext = globalContextGetter;
    sampler = getGlobalContext().sampler;
    bindState();
    
    // Trigger the loading of sounds (downloads + decoding happen asynchronously)
    startLoading();
}

SourceSound::~SourceSound ()
{
    soundLoaderThread.stopThread(20000);
    downloadTasks.clear();  // Cancels any pending downloads
}

void SourceSound::bindState ()
{
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::willBeDeleted, SourceDefaults::willBeDeleted);
    willBeDeleted.referTo(state, SourceIDs::willBeDeleted, nullptr);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::allSoundsLoaded, SourceDefaults::allSoundsLoaded);
    allSoundsLoaded.referTo(state, SourceIDs::allSoundsLoaded, nullptr);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::midiNotes, SourceDefaults::midiNotes);
    midiNotesAsString.referTo(state, SourceIDs::midiNotes, nullptr);
    
    // --> Start auto-generated code C
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::launchMode, SourceDefaults::launchMode);
    launchMode.referTo(state, SourceIDs::launchMode, nullptr, SourceDefaults::launchMode);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::startPosition, SourceDefaults::startPosition);
    startPosition.referTo(state, SourceIDs::startPosition, nullptr, SourceDefaults::startPosition);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::endPosition, SourceDefaults::endPosition);
    endPosition.referTo(state, SourceIDs::endPosition, nullptr, SourceDefaults::endPosition);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::loopStartPosition, SourceDefaults::loopStartPosition);
    loopStartPosition.referTo(state, SourceIDs::loopStartPosition, nullptr, SourceDefaults::loopStartPosition);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::loopEndPosition, SourceDefaults::loopEndPosition);
    loopEndPosition.referTo(state, SourceIDs::loopEndPosition, nullptr, SourceDefaults::loopEndPosition);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::loopXFadeNSamples, SourceDefaults::loopXFadeNSamples);
    loopXFadeNSamples.referTo(state, SourceIDs::loopXFadeNSamples, nullptr, SourceDefaults::loopXFadeNSamples);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::reverse, SourceDefaults::reverse);
    reverse.referTo(state, SourceIDs::reverse, nullptr, SourceDefaults::reverse);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::noteMappingMode, SourceDefaults::noteMappingMode);
    noteMappingMode.referTo(state, SourceIDs::noteMappingMode, nullptr, SourceDefaults::noteMappingMode);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::numSlices, SourceDefaults::numSlices);
    numSlices.referTo(state, SourceIDs::numSlices, nullptr, SourceDefaults::numSlices);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::playheadPosition, SourceDefaults::playheadPosition);
    playheadPosition.referTo(state, SourceIDs::playheadPosition, nullptr, SourceDefaults::playheadPosition);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::freezePlayheadSpeed, SourceDefaults::freezePlayheadSpeed);
    freezePlayheadSpeed.referTo(state, SourceIDs::freezePlayheadSpeed, nullptr, SourceDefaults::freezePlayheadSpeed);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::filterCutoff, SourceDefaults::filterCutoff);
    filterCutoff.referTo(state, SourceIDs::filterCutoff, nullptr, SourceDefaults::filterCutoff);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::filterRessonance, SourceDefaults::filterRessonance);
    filterRessonance.referTo(state, SourceIDs::filterRessonance, nullptr, SourceDefaults::filterRessonance);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::filterKeyboardTracking, SourceDefaults::filterKeyboardTracking);
    filterKeyboardTracking.referTo(state, SourceIDs::filterKeyboardTracking, nullptr, SourceDefaults::filterKeyboardTracking);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::filterAttack, SourceDefaults::filterAttack);
    filterAttack.referTo(state, SourceIDs::filterAttack, nullptr, SourceDefaults::filterAttack);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::filterDecay, SourceDefaults::filterDecay);
    filterDecay.referTo(state, SourceIDs::filterDecay, nullptr, SourceDefaults::filterDecay);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::filterSustain, SourceDefaults::filterSustain);
    filterSustain.referTo(state, SourceIDs::filterSustain, nullptr, SourceDefaults::filterSustain);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::filterRelease, SourceDefaults::filterRelease);
    filterRelease.referTo(state, SourceIDs::filterRelease, nullptr, SourceDefaults::filterRelease);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::filterADSR2CutoffAmt, SourceDefaults::filterADSR2CutoffAmt);
    filterADSR2CutoffAmt.referTo(state, SourceIDs::filterADSR2CutoffAmt, nullptr, SourceDefaults::filterADSR2CutoffAmt);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::gain, SourceDefaults::gain);
    gain.referTo(state, SourceIDs::gain, nullptr, SourceDefaults::gain);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::attack, SourceDefaults::attack);
    attack.referTo(state, SourceIDs::attack, nullptr, SourceDefaults::attack);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::decay, SourceDefaults::decay);
    decay.referTo(state, SourceIDs::decay, nullptr, SourceDefaults::decay);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::sustain, SourceDefaults::sustain);
    sustain.referTo(state, SourceIDs::sustain, nullptr, SourceDefaults::sustain);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::release, SourceDefaults::release);
    release.referTo(state, SourceIDs::release, nullptr, SourceDefaults::release);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::pan, SourceDefaults::pan);
    pan.referTo(state, SourceIDs::pan, nullptr, SourceDefaults::pan);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::pitch, SourceDefaults::pitch);
    pitch.referTo(state, SourceIDs::pitch, nullptr, SourceDefaults::pitch);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::pitchBendRangeUp, SourceDefaults::pitchBendRangeUp);
    pitchBendRangeUp.referTo(state, SourceIDs::pitchBendRangeUp, nullptr, SourceDefaults::pitchBendRangeUp);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::pitchBendRangeDown, SourceDefaults::pitchBendRangeDown);
    pitchBendRangeDown.referTo(state, SourceIDs::pitchBendRangeDown, nullptr, SourceDefaults::pitchBendRangeDown);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::mod2CutoffAmt, SourceDefaults::mod2CutoffAmt);
    mod2CutoffAmt.referTo(state, SourceIDs::mod2CutoffAmt, nullptr, SourceDefaults::mod2CutoffAmt);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::mod2GainAmt, SourceDefaults::mod2GainAmt);
    mod2GainAmt.referTo(state, SourceIDs::mod2GainAmt, nullptr, SourceDefaults::mod2GainAmt);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::mod2PitchAmt, SourceDefaults::mod2PitchAmt);
    mod2PitchAmt.referTo(state, SourceIDs::mod2PitchAmt, nullptr, SourceDefaults::mod2PitchAmt);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::mod2PlayheadPos, SourceDefaults::mod2PlayheadPos);
    mod2PlayheadPos.referTo(state, SourceIDs::mod2PlayheadPos, nullptr, SourceDefaults::mod2PlayheadPos);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::vel2CutoffAmt, SourceDefaults::vel2CutoffAmt);
    vel2CutoffAmt.referTo(state, SourceIDs::vel2CutoffAmt, nullptr, SourceDefaults::vel2CutoffAmt);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::vel2GainAmt, SourceDefaults::vel2GainAmt);
    vel2GainAmt.referTo(state, SourceIDs::vel2GainAmt, nullptr, SourceDefaults::vel2GainAmt);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::velSensitivity, SourceDefaults::velSensitivity);
    velSensitivity.referTo(state, SourceIDs::velSensitivity, nullptr, SourceDefaults::velSensitivity);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::midiChannel, SourceDefaults::midiChannel);
    midiChannel.referTo(state, SourceIDs::midiChannel, nullptr, SourceDefaults::midiChannel);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::pitchShift, SourceDefaults::pitchShift);
    pitchShift.referTo(state, SourceIDs::pitchShift, nullptr, SourceDefaults::pitchShift);
    SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::timeStretch, SourceDefaults::timeStretch);
    timeStretch.referTo(state, SourceIDs::timeStretch, nullptr, SourceDefaults::timeStretch);
    // --> End auto-generated code C
    
    midiCCmappings = std::make_unique<MidiCCMappingList>(state);
    
    // Build the MPE mappings used by the audio thread from the state
    updateMpeMappingsSnapshot();
}

std::vector<SourceSamplerSound*> SourceSound::getLinkedSourceSamplerSounds() {
    std::vector<SourceSamplerSound*> objects;
    for (int i=0; i<sampler->getNumSounds(); i++){
        auto* sourceSamplerSound = static_cast<SourceSamplerSound*>(sampler->getSound(i).get());
        if (sourceSamplerSound->getSourceSound() == this){
            objects.push_back(sourceSamplerSound);
        }
    }
    return objects;
}

SourceSamplerSound* SourceSound::getFirstLinkedSourceSamplerSound() {
    std::vector<SourceSamplerSound*> objects;
    for (int i=0; i<sampler->getNumSounds(); i++){
        auto* sourceSamplerSound = static_cast<SourceSamplerSound*>(sampler->getSound(i).get());
        if (sourceSamplerSound->getSourceSound() == this){
            return sourceSamplerSound;
        }
    }
    return nullptr;
}

SourceSamplerSound* SourceSound::getLinkedSourceSamplerSoundWithUUID(const juce::String& sourceSamplerSoundUUID) {
    for (int i=0; i<sampler->getNumSounds(); i++){
        auto* sourceSamplerSound = static_cast<SourceSamplerSound*>(sampler->getSound(i).get());
        if (sourceSamplerSound->getUUID() == sourceSamplerSoundUUID){
            return sourceSamplerSound;
        }
    }
    return nullptr;
}

// --------------------------------------------------------------------------------------------

juce::String SourceSound::getUUID() {
    return state.getProperty(SourceIDs::uuid).toString();
}

bool SourceSound::isScheduledForDeletion() {
    return willBeDeleted.get();
}

void SourceSound::scheduleSoundDeletion(){
    // Trigger stop all currently active notes for that sound and set timestamp so sound gets deleted async
    // Take the sampler lock so the audio thread is not rendering these voices while we stop them
    const juce::ScopedLock sl (static_cast<SourceSamplerSynthesiser*>(sampler)->getVoicesLock());
    for (int i=0; i<sampler->getNumVoices(); i++){
        auto* voice = sampler->getVoice(i);
        if (voice != nullptr){
            if (voice->isVoiceActive()){
                auto* currentlyPlayingSound = static_cast<SourceSamplerVoice*>(voice)->getCurrentlyPlayingSourceSamplerSound();
                if (currentlyPlayingSound != nullptr && currentlyPlayingSound->getSourceSound() == this){
                    voice->stopNote(0.0f, false);
                }
            }
        }
    }
    willBeDeleted = true;
    scheduledForDeletionTime = juce::Time::getMillisecondCounterHiRes();
}

bool SourceSound::shouldBeDeleted(){
    return isScheduledForDeletion() && ((juce::Time::getMillisecondCounterHiRes() - scheduledForDeletionTime) > SAFE_SOUND_DELETION_TIME_MS);
}

// --------------------------------------------------------------------------------------------

int SourceSound::getParameterInt(juce::Identifier identifier){
    // --> Start auto-generated code E
        if (identifier == SourceIDs::launchMode) { return launchMode.get(); }
        else if (identifier == SourceIDs::loopXFadeNSamples) { return loopXFadeNSamples.get(); }
        else if (identifier == SourceIDs::reverse) { return reverse.get(); }
        else if (identifier == SourceIDs::noteMappingMode) { return noteMappingMode.get(); }
        else if (identifier == SourceIDs::numSlices) { return numSlices.get(); }
        else if (identifier == SourceIDs::midiChannel) { return midiChannel.get(); }
        // --> End auto-generated code E
    jassertfalse; // No int parameter with this name
    return 0;
}

float SourceSound::getParameterFloat(juce::Identifier identifier, bool normed){
    // --> Start auto-generated code F
        if (identifier == SourceIDs::startPosition) { return !normed ? startPosition.get() : juce::jmap(startPosition.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (identifier == SourceIDs::endPosition) { return !normed ? endPosition.get() : juce::jmap(endPosition.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (identifier == SourceIDs::loopStartPosition) { return !normed ? loopStartPosition.get() : juce::jmap(loopStartPosition.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (identifier == SourceIDs::loopEndPosition) { return !normed ? loopEndPosition.get() : juce::jmap(loopEndPosition.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (identifier == SourceIDs::playheadPosition) { return !normed ? playheadPosition.get() : juce::jmap(playheadPosition.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (identifier == SourceIDs::freezePlayheadSpeed) { return !normed ? freezePlayheadSpeed.get() : juce::jmap(freezePlayheadSpeed.get(), 1.0f, 5000.0f, 0.0f, 1.0f); }
        else if (identifier == SourceIDs::filterCutoff) { return !normed ? filterCutoff.get() : juce::jmap(filterCutoff.get(), 10.0f, 20000.0f, 0.0f, 1.0f); }
        else if (identifier == SourceIDs::filterRessonance) { return !normed ? filterRessonance.get() : juce::jmap(filterRessonance.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (identifier == SourceIDs::filterKeyboardTracking) { return !normed ? filterKeyboardTracking.get() : juce::jmap(filterKeyboardTracking.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (identifier == SourceIDs::filterAttack) { return !normed ? filterAttack.get() : juce::jmap(filterAttack.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (identifier == SourceIDs::filterDecay) { return !normed ? filterDecay.get() : juce::jmap(filterDecay.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (identifier == SourceIDs::filterSustain) { return !normed ? filterSustain.get() : juce::jmap(filterSustain.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (identifier == SourceIDs::filterRelease) { return !normed ? filterRelease.get() : juce::jmap(filterRelease.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (identifier == SourceIDs::filterADSR2CutoffAmt) { return !normed ? filterADSR2CutoffAmt.get() : juce::jmap(filterADSR2CutoffAmt.get(), 0.0f, 100.0f, 0.0f, 1.0f); }
        else if (identifier == SourceIDs::gain) { return !normed ? gain.get() : juce::jmap(gain.get(), -80.0f, 12.0f, 0.0f, 1.0f); }
        else if (identifier == SourceIDs::attack) { return !normed ? attack.get() : juce::jmap(attack.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (identifier == SourceIDs::decay) { return !normed ? decay.get() : juce::jmap(decay.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (identifier == SourceIDs::sustain) { return !normed ? sustain.get() : juce::jmap(sustain.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (identifier == SourceIDs::release) { return !normed ? release.get() : juce::jmap(release.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (identifier == SourceIDs::pan) { return !normed ? pan.get() : juce::jmap(pan.get(), -1.0f, 1.0f, 0.0f, 1.0f); }
        else if (identifier == SourceIDs::pitch) { return !normed ? pitch.get() : juce::jmap(pitch.get(), -36.0f, 36.0f, 0.0f, 1.0f); }
        else if (identifier == SourceIDs::pitchBendRangeUp) { return !normed ? pitchBendRangeUp.get() : juce::jmap(pitchBendRangeUp.get(), 0.0f, 36.0f, 0.0f, 1.0f); }
        else if (identifier == SourceIDs::pitchBendRangeDown) { return !normed ? pitchBendRangeDown.get() : juce::jmap(pitchBendRangeDown.get(), 0.0f, 36.0f, 0.0f, 1.0f); }
        else if (identifier == SourceIDs::mod2CutoffAmt) { return !normed ? mod2CutoffAmt.get() : juce::jmap(mod2CutoffAmt.get(), 0.0f, 100.0f, 0.0f, 1.0f); }
        else if (identifier == SourceIDs::mod2GainAmt) { return !normed ? mod2GainAmt.get() : juce::jmap(mod2GainAmt.get(), -12.0f, 12.0f, 0.0f, 1.0f); }
        else if (identifier == SourceIDs::mod2PitchAmt) { return !normed ? mod2PitchAmt.get() : juce::jmap(mod2PitchAmt.get(), -12.0f, 12.0f, 0.0f, 1.0f); }
        else if (identifier == SourceIDs::mod2PlayheadPos) { return !normed ? mod2PlayheadPos.get() : juce::jmap(mod2PlayheadPos.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (identifier == SourceIDs::vel2CutoffAmt) { return !normed ? vel2CutoffAmt.get() : juce::jmap(vel2CutoffAmt.get(), 0.0f, 100.0f, 0.0f, 1.0f); }
        else if (identifier == SourceIDs::vel2GainAmt) { return !normed ? vel2GainAmt.get() : juce::jmap(vel2GainAmt.get(), 0.0f, 1.0f, 0.0f, 1.0f); }
        else if (identifier == SourceIDs::velSensitivity) { return !normed ? velSensitivity.get() : juce::jmap(velSensitivity.get(), 0.0f, 6.0f, 0.0f, 1.0f); }
        else if (identifier == SourceIDs::pitchShift) { return !normed ? pitchShift.get() : juce::jmap(pitchShift.get(), -36.0f, 36.0f, 0.0f, 1.0f); }
        else if (identifier == SourceIDs::timeStretch) { return !normed ? timeStretch.get() : juce::jmap(timeStretch.get(), 0.1f, 4.0f, 0.0f, 1.0f); }
        // --> End auto-generated code F
    jassertfalse; // No float parameter with this name
    return 0.0f;
}

void SourceSound::setParameterByNameFloat(juce::Identifier identifier, float value, bool normed){
    // --> Start auto-generated code B
        if (identifier == SourceIDs::startPosition) { startPosition = !normed ? juce::jlimit(0.0f, 1.0f, value) : juce::jmap(value, 0.0f, 1.0f); }
        else if (identifier == SourceIDs::endPosition) { endPosition = !normed ? juce::jlimit(0.0f, 1.0f, value) : juce::jmap(value, 0.0f, 1.0f); }
        else if (identifier == SourceIDs::loopStartPosition) { loopStartPosition = !normed ? juce::jlimit(0.0f, 1.0f, value) : juce::jmap(value, 0.0f, 1.0f); }
        else if (identifier == SourceIDs::loopEndPosition) { loopEndPosition = !normed ? juce::jlimit(0.0f, 1.0f, value) : juce::jmap(value, 0.0f, 1.0f); }
        else if (identifier == SourceIDs::playheadPosition) { playheadPosition = !normed ? juce::jlimit(0.0f, 1.0f, value) : juce::jmap(value, 0.0f, 1.0f); }
        else if (identifier == SourceIDs::freezePlayheadSpeed) { freezePlayheadSpeed = !normed ? juce::jlimit(1.0f, 5000.0f, value) : juce::jmap(value, 1.0f, 5000.0f); }
        else if (identifier == SourceIDs::filterCutoff) { filterCutoff = !normed ? juce::jlimit(10.0f, 20000.0f, value) : juce::jmap(value, 10.0f, 20000.0f); }
        else if (identifier == SourceIDs::filterRessonance) { filterRessonance = !normed ? juce::jlimit(0.0f, 1.0f, value) : juce::jmap(value, 0.0f, 1.0f); }
        else if (identifier == SourceIDs::filterKeyboardTracking) { filterKeyboardTracking = !normed ? juce::jlimit(0.0f, 1.0f, value) : juce::jmap(value, 0.0f, 1.0f); }
        else if (identifier == SourceIDs::filterAttack) { filterAttack = !normed ? juce::jlimit(0.0f, 1.0f, value) : juce::jmap(value, 0.0f, 1.0f); }
        else if (identifier == SourceIDs::filterDecay) { filterDecay = !normed ? juce::jlimit(0.0f, 1.0f, value) : juce::jmap(value, 0.0f, 1.0f); }
        else if (identifier == SourceIDs::filterSustain) { filterSustain = !normed ? juce::jlimit(0.0f, 1.0f, value) : juce::jmap(value, 0.0f, 1.0f); }
        else if (identifier == SourceIDs::filterRelease) { filterRelease = !normed ? juce::jlimit(0.0f, 1.0f, value) : juce::jmap(value, 0.0f, 1.0f); }
        else if (identifier == SourceIDs::filterADSR2CutoffAmt) { filterADSR2CutoffAmt = !normed ? juce::jlimit(0.0f, 100.0f, value) : juce::jmap(value, 0.0f, 100.0f); }
        else if (identifier == SourceIDs::gain) { gain = !normed ? juce::jlimit(-80.0f, 12.0f, value) : juce::jmap(value, -80.0f, 12.0f); }
        else if (identifier == SourceIDs::attack) { attack = !normed ? juce::jlimit(0.0f, 1.0f, value) : juce::jmap(value, 0.0f, 1.0f); }
        else if (identifier == SourceIDs::decay) { decay = !normed ? juce::jlimit(0.0f, 1.0f, value) : juce::jmap(value, 0.0f, 1.0f); }
        else if (identifier == SourceIDs::sustain) { sustain = !normed ? juce::jlimit(0.0f, 1.0f, value) : juce::jmap(value, 0.0f, 1.0f); }
        else if (identifier == SourceIDs::release) { release = !normed ? juce::jlimit(0.0f, 1.0f, value) : juce::jmap(value, 0.0f, 1.0f); }
        else if (identifier == SourceIDs::pan) { pan = !normed ? juce::jlimit(-1.0f, 1.0f, value) : juce::jmap(value, -1.0f, 1.0f); }
        else if (identifier == SourceIDs::pitch) { pitch = !normed ? juce::jlimit(-36.0f, 36.0f, value) : juce::jmap(value, -36.0f, 36.0f); }
        else if (identifier == SourceIDs::pitchBendRangeUp) { pitchBendRangeUp = !normed ? juce::jlimit(0.0f, 36.0f, value) : juce::jmap(value, 0.0f, 36.0f); }
        else if (identifier == SourceIDs::pitchBendRangeDown) { pitchBendRangeDown = !normed ? juce::jlimit(0.0f, 36.0f, value) : juce::jmap(value, 0.0f, 36.0f); }
        else if (identifier == SourceIDs::mod2CutoffAmt) { mod2CutoffAmt = !normed ? juce::jlimit(0.0f, 100.0f, value) : juce::jmap(value, 0.0f, 100.0f); }
        else if (identifier == SourceIDs::mod2GainAmt) { mod2GainAmt = !normed ? juce::jlimit(-12.0f, 12.0f, value) : juce::jmap(value, -12.0f, 12.0f); }
        else if (identifier == SourceIDs::mod2PitchAmt) { mod2PitchAmt = !normed ? juce::jlimit(-12.0f, 12.0f, value) : juce::jmap(value, -12.0f, 12.0f); }
        else if (identifier == SourceIDs::mod2PlayheadPos) { mod2PlayheadPos = !normed ? juce::jlimit(0.0f, 1.0f, value) : juce::jmap(value, 0.0f, 1.0f); }
        else if (identifier == SourceIDs::vel2CutoffAmt) { vel2CutoffAmt = !normed ? juce::jlimit(0.0f, 100.0f, value) : juce::jmap(value, 0.0f, 100.0f); }
        else if (identifier == SourceIDs::vel2GainAmt) { vel2GainAmt = !normed ? juce::jlimit(0.0f, 1.0f, value) : juce::jmap(value, 0.0f, 1.0f); }
        else if (identifier == SourceIDs::velSensitivity) { velSensitivity = !normed ? juce::jlimit(0.0f, 6.0f, value) : juce::jmap(value, 0.0f, 6.0f); }
        else if (identifier == SourceIDs::pitchShift) { pitchShift = !normed ? juce::jlimit(-36.0f, 36.0f, value) : juce::jmap(value, -36.0f, 36.0f); }
        else if (identifier == SourceIDs::timeStretch) { timeStretch = !normed ? juce::jlimit(0.1f, 4.0f, value) : juce::jmap(value, 0.1f, 4.0f); }
        // --> End auto-generated code B
    else { DBG("No float parameter with name " << identifier.toString()); return; }
    
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
    
    // If setting pitch shift/time stretch properties, also trigger re pre-processing of audio data
    if ((identifier == SourceIDs::pitchShift) || (identifier == SourceIDs::timeStretch)) {
        for (auto sourceSamplerSound: getLinkedSourceSamplerSounds()){
            sourceSamplerSound->setStretchParameters(pitchShift, timeStretch);
        }
    }
}

void SourceSound::setParameterByNameInt(juce::Identifier identifier, int value){
    // --> Start auto-generated code D
        if (identifier == SourceIDs::launchMode) { launchMode = juce::jlimit(0, 4, value); }
        else if (identifier == SourceIDs::loopXFadeNSamples) { loopXFadeNSamples = juce::jlimit(10, 100000, value); }
        else if (identifier == SourceIDs::reverse) { reverse = juce::jlimit(0, 1, value); }
        else if (identifier == SourceIDs::noteMappingMode) { noteMappingMode = juce::jlimit(0, 3, value); }
        else if (identifier == SourceIDs::numSlices) { numSlices = juce::jlimit(0, 100, value); }
        else if (identifier == SourceIDs::midiChannel) { midiChannel = juce::jlimit(0, 16, value); }
        // --> End auto-generated code D
    else { DBG("No int parameter with name " << identifier.toString()); return; }
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
                int bitRangeStart = i * numVelocitiesPerLayer ;
                int numBitsToSet = numVelocitiesPerLayer;
                if (i == numLayers - 1){
                    // If this is the last layer, make sure numBitsToSet is calculated so that we use the full velocity scale (up to 127)
                    numBitsToSet = 128 - bitRangeStart;
                }
                midiVelocities.setRange(bitRangeStart, numBitsToSet, true);
                int n= 0;
                for (auto sourceSamplerSound: getLinkedSourceSamplerSounds()){
                    if ((sourceSamplerSound->getMidiRootNote() == rootNote) && (sourceSamplerSound->getMidiVelocityLayer() == rootNoteVelocityLayers[i])){
                        // Assign the computed midi velocities to SourceSamplerSound(s) with that root note and velocity layer (there should be only one)
                        sourceSamplerSound->setMappedVelocities(midiVelocities);
                        n+=1;
                    }
                }
            }
        }
    }
}

void SourceSound::setMidiRootNote(int newMidiRootNote){
    if (getLinkedSourceSamplerSounds().size() == 0){
        // If sound has not yer any liked SourceSamplerSound objects, set midi property to thge first SOUND_SAMPLE in the sound's state
        auto firstSourceSamplerSoundState = state.getChild(0);
        if (firstSourceSamplerSoundState.isValid()){
            firstSourceSamplerSoundState.setProperty(SourceIDs::midiRootNote, newMidiRootNote, nullptr);
        }
    } else if (getLinkedSourceSamplerSounds().size() == 1) {
        // If sound already has a SourceSamplerSound object created, set the midi root note (to the first only)
        getFirstLinkedSourceSamplerSound()->setMidiRootNote(newMidiRootNote);
    } else {
        // If the sound has more than one SourceSamplerSound, no root note is updated as we don't know which sound should be updated
    }
}

int SourceSound::getMidiNoteFromFirstSourceSamplerSound(){
    // Get the midi root note of the first SourceSamplerSound form the linked object if already exists, or from the state (useful if SourceSamplerSound objects have not yet been created)
    if (getLinkedSourceSamplerSounds().size() == 0){
        auto firstSourceSamplerSoundState = state.getChild(0);
        if (firstSourceSamplerSoundState.isValid()){
            return firstSourceSamplerSoundState.getProperty(SourceIDs::midiRootNote, SourceDefaults::midiRootNote);
        }
    } else {
        return getFirstLinkedSourceSamplerSound()->getMidiRootNote();
    }
    return SourceDefaults::midiRootNote;
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
    const juce::ScopedLock sl (midiMappingCreateDeleteLock);
    
    juce::ValueTree existingMapping = state.getChildWithProperty(SourceIDs::uuid, uuid);
    if (existingMapping.isValid()) {
        // Modify existing mapping
        existingMapping.setProperty (SourceIDs::ccNumber, ccNumber, nullptr);
        existingMapping.setProperty (SourceIDs::parameterName, parameterName, nullptr);
        existingMapping.setProperty (SourceIDs::minRange, juce::jlimit(0.0f, 1.0f, minRange), nullptr);
        existingMapping.setProperty (SourceIDs::maxRange, juce::jlimit(0.0f, 1.0f, maxRange), nullptr);
    } else {
        // No mapping with uuid found, create a new one
        // TODO: move this to the MidiCCMappingList class?
        juce::ValueTree newMapping = SourceHelpers::createMidiMappingState(ccNumber, parameterName, juce::jlimit(0.0f, 1.0f, minRange), juce::jlimit(0.0f, 1.0f, maxRange));
        state.addChild(newMapping, -1, nullptr);
    }
}

void SourceSound::addOrEditMpeMapping(const juce::String& uuid, const juce::String& source, const juce::String& parameterName, float minRange, float maxRange, float curve){
    if (SourceHelpers::mpeSourceFromName(source) == SourceHelpers::MpeSource::numSources){
        DBG("Invalid MPE source " << source);
        return;
    }
    const auto* parameterInfo = findSoundParameterInfo(parameterName);
    if (parameterInfo == nullptr || parameterInfo->isInt){
        DBG("Invalid parameter for MPE mapping " << parameterName);
        return;
    }
    minRange = juce::jlimit(0.0f, 1.0f, minRange);
    maxRange = juce::jlimit(0.0f, 1.0f, maxRange);
    curve = juce::jlimit(-1.0f, 1.0f, curve);
    juce::ValueTree existingMapping = state.getChildWithProperty(SourceIDs::uuid, uuid);
    if (existingMapping.isValid() && existingMapping.hasType(SourceIDs::MPE_MAPPING)){
        existingMapping.setProperty(SourceIDs::mpeSource, source, nullptr);
        existingMapping.setProperty(SourceIDs::parameterName, parameterName, nullptr);
        existingMapping.setProperty(SourceIDs::minRange, minRange, nullptr);
        existingMapping.setProperty(SourceIDs::maxRange, maxRange, nullptr);
        existingMapping.setProperty(SourceIDs::curve, curve, nullptr);
    } else {
        int numExisting = 0;
        for (const auto& child: state)
            if (child.hasType(SourceIDs::MPE_MAPPING)) numExisting += 1;
        if (numExisting >= maxMpeMappings){
            DBG("Maximum number of MPE mappings reached");
            return;
        }
        auto newMapping = SourceHelpers::createMpeMappingState(source, parameterName, minRange, maxRange, curve);
        if (uuid.isNotEmpty())
            newMapping.setProperty(SourceIDs::uuid, uuid, nullptr);
        state.addChild(newMapping, -1, nullptr);
    }
    updateMpeMappingsSnapshot();
}

void SourceSound::removeMpeMapping(const juce::String& uuid){
    auto mapping = state.getChildWithProperty(SourceIDs::uuid, uuid);
    if (mapping.isValid() && mapping.hasType(SourceIDs::MPE_MAPPING))
        state.removeChild(mapping, nullptr);
    updateMpeMappingsSnapshot();
}

void SourceSound::updateMpeMappingsSnapshot(){
    // Build the new list outside the lock, then copy it while holding the lock (the audio thread only try-locks)
    MpeMappings newMappings;
    int numNewMappings = 0;
    for (const auto& child: state){
        if (!child.hasType(SourceIDs::MPE_MAPPING) || numNewMappings >= maxMpeMappings)
            continue;
        const auto source = SourceHelpers::mpeSourceFromName(child.getProperty(SourceIDs::mpeSource).toString());
        const auto* parameterInfo = findSoundParameterInfo(child.getProperty(SourceIDs::parameterName).toString());
        if (source == SourceHelpers::MpeSource::numSources || parameterInfo == nullptr || parameterInfo->isInt)
            continue;
        auto& mapping = newMappings[(size_t)numNewMappings++];
        mapping.source = source;
        mapping.parameter = parameterInfo->getIdentifier();
        mapping.minRange = child.getProperty(SourceIDs::minRange, 0.0f);
        mapping.maxRange = child.getProperty(SourceIDs::maxRange, 1.0f);
        mapping.curve = child.getProperty(SourceIDs::curve, SourceDefaults::mpeCurve);
        mapping.parameterMin = parameterInfo->minValue;
        mapping.parameterMax = parameterInfo->maxValue;
    }
    const juce::SpinLock::ScopedLockType sl (mpeMappingsLock);
    mpeMappings = newMappings;
    numMpeMappings = numNewMappings;
}

int SourceSound::copyMpeMappings(MpeMappings& dest) noexcept{
    const juce::SpinLock::ScopedTryLockType tl (mpeMappingsLock);
    if (!tl.isLocked())
        return -1;
    for (int i = 0; i < numMpeMappings; ++i)
        dest[(size_t)i] = mpeMappings[(size_t)i];
    return numMpeMappings;
}

std::vector<MidiCCMapping*> SourceSound::getMidiMappingsForCcNumber(int ccNumber){
    std::vector<MidiCCMapping*> toReturn = {};
    for (auto* midiCCmapping: midiCCmappings->objects){
        if (midiCCmapping->ccNumber == ccNumber){
            toReturn.push_back(midiCCmapping);
        }
    }
    return toReturn;
}

void SourceSound::removeMidiMapping(juce::String uuid){
    const juce::ScopedLock sl (midiMappingCreateDeleteLock);
    midiCCmappings->removeMidiCCMappingWithUUID(uuid);
}

void SourceSound::applyMidiCCModulations(int channel, int number, int value) {
    // NOTE: this modifies the state so it must be called from the message thread
    const juce::ScopedLock sl (midiMappingCreateDeleteLock);
    
    int globalMidiChannel = getGlobalContext().midiInChannel;
    int soundMidiChannel = midiChannel.get();
    bool appliesToChannel = false;
    if (soundMidiChannel == 0){
        // Check with global
        appliesToChannel = (globalMidiChannel == 0) || (globalMidiChannel == channel);
    } else {
        // Check with sound channel
        appliesToChannel = soundMidiChannel == channel;
    }
    if (appliesToChannel){
        std::vector<MidiCCMapping*> mappings = getMidiMappingsForCcNumber(number);
        for (int i=0; i<mappings.size(); i++){
            float normInputValue = (float)value/127.0;  // This goes from 0 to 1
            float value = juce::jmap(normInputValue, mappings[i]->minRange.get(), mappings[i]->maxRange.get());
            setParameterByNameFloat(mappings[i]->parameterName.get(), value, true);
        }
    }
};

// ------------------------------------------------------------------------------------------------

bool SourceSound::sourceSamplerSoundWithUUIDAlreadyCreated(const juce::String& sourceSamplerSoundUUID)
{
    for (auto sourceSamplerSound: getLinkedSourceSamplerSounds()){
        if (sourceSamplerSound->getUUID() == sourceSamplerSoundUUID){
            return true;
        }
    }
    return false;
}

void SourceSound::removeSourceSampleSoundsFromSampler()
{
    const juce::ScopedLock sl (samplerSoundCreateDeleteLock);
    std::vector<int> soundIndexesToDelete;
    for (int i=0; i<sampler->getNumSounds(); i++){
        auto* sourceSamplerSound = static_cast<SourceSamplerSound*>(sampler->getSound(i).get());
        if (sourceSamplerSound->getSourceSound() == this){
            // If the pointer to sourceSound of the sourceSamplerSound is the current sourceSound, then the sound should be deleted
            soundIndexesToDelete.push_back(i);
        }
    }
    
    int numDeleted = 0;
    for (auto idx: soundIndexesToDelete)
    {
        sampler->removeSound(idx - numDeleted);  // Compensate index updates as sounds get removed
        numDeleted += 1;
    }
    DBG("Removed " << numDeleted << " SourceSamplerSound(s) from sampler... ");
}

void SourceSound::removeSourceSamplerSound(const juce::String& samplerSoundUUID, int indexInSampler)
{
    const juce::ScopedLock sl (samplerSoundCreateDeleteLock);
    sampler->removeSound(indexInSampler); // Remove source sampler sound from the sampler
    state.removeChild(state.getChildWithProperty(SourceIDs::uuid, samplerSoundUUID), nullptr);  // Also remove the bit of state corresponding to the sampler sound
    DBG("Removed 1 SourceSamplerSound(s) from sampler... ");
}

void SourceSound::addNewSourceSamplerSoundFromValueTree(juce::ValueTree newSourceSamplerSound)
{
    // Add the new sample sound data to the state and then re-start the loading process that will load the new sound
    // and later create SourceSamplerSound for it (already loaded samples are skipped)
    state.addChild(newSourceSamplerSound, -1, nullptr);
    startLoading();
}

void SourceSound::scheduleDeletionOfSourceSamplerSound(const juce::String& sourceSamplerSoundUUID)
{
    // Stop all notes currently being played from that sound
    auto* sourceSamplerSound = getLinkedSourceSamplerSoundWithUUID(sourceSamplerSoundUUID);
    if (sourceSamplerSound != nullptr){
        const juce::ScopedLock sl (static_cast<SourceSamplerSynthesiser*>(sampler)->getVoicesLock());
        for (int i=0; i<sampler->getNumVoices(); i++){
            auto* voice = sampler->getVoice(i);
            if (voice != nullptr){
                if (voice->isVoiceActive()){
                    auto* currentlyPlayingSound = static_cast<SourceSamplerVoice*>(voice)->getCurrentlyPlayingSourceSamplerSound();
                    if (currentlyPlayingSound == sourceSamplerSound){
                        voice->stopNote(0.0f, false);
                    }
                }
            }
        }
        sourceSamplerSound->scheduleSampleSoundDeletion();
    }
}


// ------------------------------------------------------------------------------------------------

void SourceSound::postToMessageThread(std::function<void(SourceSound&)> callback)
{
    // Can be called from any thread. The callback is only run if the SourceSound object still exists.
    juce::WeakReference<SourceSound> weak = weakThis;
    juce::MessageManager::callAsync([weak, callback = std::move(callback)]{
        if (auto* sound = weak.get())
            callback(*sound);
    });
}

void SourceSound::startLoading()
{
    // Loading happens in a background thread which works on a copy of the state (ValueTree objects are not thread safe).
    // All modifications to the actual state are posted back to the message thread.
    if (soundLoaderThread.isThreadRunning()){
        // If the loader thread is running, stop it first (give it some time so it can finish what it was doing)
        soundLoaderThread.stopThread(10000);
    }
    allSoundsLoaded = false;
    loaderStateSnapshot = state.createCopy();
    loaderContext = getGlobalContext();
    soundLoaderThread.startThread();
}

void SourceSound::loadSounds(std::function<bool()> shouldStopLoading)
{
    // Runs in the loader thread. Load all the SourceSamplerSound(s) of SourceSound, triggering downloading if needed. This will normally be one
    // single sound except for the case of multi-sample sounds in which there might be different sounds per pitch and velocity layers.
    // IMPORTANT: this method must not read or write the live state (only loaderStateSnapshot/loaderContext)
    
    const GlobalContextStruct& context = loaderContext;
    std::vector<PendingDownload> pendingDownloads;
    
    // Helper to set a property both in the snapshot (so later steps see it) and in the real state (on the message thread)
    auto setSampleProperty = [this](juce::ValueTree& child, const juce::Identifier& property, const juce::var& value){
        child.setProperty(property, value, nullptr);
        juce::String sampleUUID = child.getProperty(SourceIDs::uuid).toString();
        postToMessageThread([sampleUUID, property, value](SourceSound& sound){
            auto liveChild = sound.state.getChildWithProperty(SourceIDs::uuid, sampleUUID);
            if (liveChild.isValid())
                liveChild.setProperty(property, value, nullptr);
        });
    };
    
    // Iterate thorugh all sampler sounds and trigger downloads if necessary
    bool allAlreadyDownloaded = true;
    for (int i=0; i<loaderStateSnapshot.getNumChildren(); i++){
        
        if (shouldStopLoading()){
            DBG("Cancelled loading sounds process");
            // At every iteration, check if the loading process should be stopped (e.g. because the thread was stopped), and do early return if so
            return;
        }
        
        auto child = loaderStateSnapshot.getChild(i);
        if (child.hasType(SourceIDs::SOUND_SAMPLE)){
            bool isFromFreesound = child.getProperty(SourceIDs::soundFromFreesound, false);
            juce::String filePath = child.getProperty(SourceIDs::filePath, ""); // Relative file path
            juce::File locationInDisk; // The location in disk where sound should be downloaded/placed
            
            if (isFromFreesound){
                // Check if sound already exists in disk, otherwise trigger download
                // If sound is from Freesound, compute the expected file location and update the state with that location (it will most likely be the same)
                // This can be useful if sharing presets that had downloaded original quality files with plugin instances that don't have the ability to
                // download original quality files (e.g. have no access token set)
                locationInDisk = getFreesoundFileLocation(child, context);
                juce::String relativePath = locationInDisk.getRelativePathFrom(context.sourceDataLocation);
                if (relativePath != filePath)
                    setSampleProperty(child, SourceIDs::filePath, relativePath);
                if (fileAlreadyInDisk(locationInDisk)){
                    // If file already exists at the expected location, mark it as downloaded and don't trigger downloading
                    setSampleProperty(child, SourceIDs::usesPreview, !locationInDisk.getFullPathName().contains("-original"));
                    setSampleProperty(child, SourceIDs::downloadProgress, 100.0);
                    setSampleProperty(child, SourceIDs::downloadCompleted, true);
                } else {
                    // If the sound does not exist, then trigger the download
                    allAlreadyDownloaded = false;
                    setSampleProperty(child, SourceIDs::downloadProgress, 0.0);
                    setSampleProperty(child, SourceIDs::downloadCompleted, false);
                    juce::String downloadURL;
                    juce::String downloadHeaders;
                    if (shouldUseOriginalQualityFile(child, context)){
                        // Download original quality file
                        downloadURL = juce::String("https://freesound.org/apiv2/sounds/<sound_id>/download/").replace("<sound_id>", child.getProperty(SourceIDs::soundId).toString(), false);
                        downloadHeaders = "Authorization: Bearer " + context.freesoundOauthAccessToken;
                        setSampleProperty(child, SourceIDs::usesPreview, false);
                    } else {
                        // Download preview quality file
                        downloadURL = child.getProperty(SourceIDs::previewURL, "").toString();
                        setSampleProperty(child, SourceIDs::usesPreview, true);
                    }
                    # if !USE_EXTERNAL_HTTP_SERVER_FOR_DOWNLOADS
                    pendingDownloads.push_back({downloadURL, locationInDisk, downloadHeaders});
                    # else
                    // In ELK builds, downloads are done by an external HTTP server which notifies the plugin when finished
                    juce::URL downloadServerUrlEndpoint = juce::URL("http://localhost:" + juce::String(HTTP_DOWNLOAD_SERVER_PORT) + "/download_sound").withParameter("soundUUID", child.getProperty(SourceIDs::uuid).toString()).withParameter("urlToDownloadFrom", downloadURL).withParameter("pathToSaveDownloadedFile", locationInDisk.getFullPathName()).withParameter("downloadHeaders", downloadHeaders);
                    int statusCode = -1;
                    juce::StringPairArray responseHeaders;
                    auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress).withConnectionTimeoutMs (MAX_DOWNLOAD_WAITING_TIME_MS).withResponseHeaders (&responseHeaders).withStatusCode (&statusCode);
                    if (auto stream = downloadServerUrlEndpoint.createInputStream(options)){
                        juce::String resp = stream->readEntireStreamAsString();
                        DBG("Downloading sound to " << locationInDisk.getFullPathName() << " with external server: " + (juce::String)statusCode + ": " + resp);
                    } else {
                        DBG("Download server ERROR downloading sound to " << locationInDisk.getFullPathName());
                    }
                    # endif
                }
            } else {
                // If sound not from Freesound
                // Chek if sound already exists in disk, otherwise there's nothing we can do as the sound is not from Freesound we can't re-download it
                if (filePath != ""){
                    locationInDisk = context.sourceDataLocation.getChildFile(filePath);
                    if (fileAlreadyInDisk(locationInDisk)){
                        setSampleProperty(child, SourceIDs::downloadProgress, 100.0);
                        setSampleProperty(child, SourceIDs::downloadCompleted, true);
                    }
                }
            }
        }
    }
    
    if (!pendingDownloads.empty()){
        // Downloads are started (and owned) from the message thread
        postToMessageThread([pendingDownloads](SourceSound& sound){ sound.startDownloads(pendingDownloads); });
    }
    
    if (!allAlreadyDownloaded){
        // Wait until downloads finish, downloadFinished will re-start the loading process
        return;
    }
    
    // All files are available: decode them (here in the loader thread) and pass the decoded audio to the message thread
    // where SourceSamplerSound objects will be created and added to the sampler
    juce::AudioFormatManager audioFormatManager;
    audioFormatManager.registerBasicFormats();
    auto loadedSamples = std::make_shared<std::vector<LoadedSample>>();
    for (int i=0; i<loaderStateSnapshot.getNumChildren(); i++){
        if (shouldStopLoading()){
            DBG("Cancelled loading sounds process at sound " << i);
            return;
        }
        auto child = loaderStateSnapshot.getChild(i);
        if (!child.hasType(SourceIDs::SOUND_SAMPLE))
            continue;
        juce::File locationInDisk = context.sourceDataLocation.getChildFile(child.getProperty(SourceIDs::filePath, "").toString());
        if (!(fileAlreadyInDisk(locationInDisk) && fileLocationIsSupportedAudioFileFormat(locationInDisk)))
            continue;
        std::unique_ptr<juce::AudioFormatReader> reader (audioFormatManager.createReaderFor(locationInDisk));
        if (reader == nullptr || reader->sampleRate <= 0 || reader->lengthInSamples <= 0){
            // If for some reason the reader is NULL (corrupted file for example), don't proceed creating the SourceSamplerSound
            // If the file was downloaded from Freesound, delete it so it can be re-downloaded (never delete user's own files)
            DBG("Skipping loading of possibly corrupted file: " << locationInDisk.getFullPathName());
            if ((bool) child.getProperty(SourceIDs::soundFromFreesound, false))
                locationInDisk.deleteFile();
            continue;
        }
        const int numSamples = (int) juce::jmin (reader->lengthInSamples, (juce::int64) (MAX_SAMPLE_LENGTH * reader->sampleRate));
        auto buffer = std::make_unique<juce::AudioBuffer<float>> ((int) juce::jmin (2u, reader->numChannels), numSamples);
        reader->read (buffer.get(), 0, numSamples, 0, true, true);
        
        juce::String sampleUUID = child.getProperty(SourceIDs::uuid).toString();
        
        // Write low resolution version of the audio to disk so it can be used in the web UI for displaying waveforms
        SourceSamplerSound::writeWaveformFileToDisk (*buffer, reader->sampleRate, context.tmpFilesLocation.getChildFile(sampleUUID).withFileExtension("wav"));
        
        loadedSamples->push_back({sampleUUID, std::move(buffer), reader->sampleRate});
    }
    
    postToMessageThread([loadedSamples](SourceSound& sound){ sound.onSamplesLoaded(*loadedSamples); });
}

void SourceSound::startDownloads(const std::vector<PendingDownload>& downloads)
{
    for (const auto& download: downloads){
        auto options = juce::URL::DownloadTaskOptions().withListener(this);
        if (download.extraHeaders.isNotEmpty())
            options = options.withExtraHeaders(download.extraHeaders);
        if (SourceHelpers::canUseSharedContainerForDownloads())
            options = options.withSharedContainer(APP_GROUP_ID);
        download.location.getParentDirectory().createDirectory();
        if (auto downloadTask = juce::URL(download.url).downloadToFile(download.location, options)){
            downloadTasks.push_back(std::move(downloadTask));
            DBG("Downloading sound to " << download.location.getFullPathName());
        } else {
            DBG("Could not start download of " << download.url);
            downloadFinished(download.location, false);
        }
    }
}

void SourceSound::onSamplesLoaded(std::vector<LoadedSample>& loadedSamples)
{
    // Runs in the message thread: create SourceSamplerSound objects from decoded audio and add them to the sampler
    if (isScheduledForDeletion())
        return;
    
    const juce::ScopedLock sl (samplerSoundCreateDeleteLock);
    const GlobalContextStruct context = getGlobalContext();
    int numAdded = 0;
    for (auto& loadedSample: loadedSamples){
        auto child = state.getChildWithProperty(SourceIDs::uuid, loadedSample.uuid);
        if (!child.isValid() || sourceSamplerSoundWithUUIDAlreadyCreated(loadedSample.uuid))
            continue;  // Sample was removed in the meantime or already loaded
        auto* sourceSamplerSound = new SourceSamplerSound(child, this, std::move(loadedSample.buffer), loadedSample.sampleRate, context.sampleRate, context.samplesPerBlock);
        sampler->addSound(sourceSamplerSound);
        numAdded += 1;
    }
    assignMidiNotesAndVelocityToSourceSamplerSounds();
    DBG("Added " << numAdded << " SourceSamplerSound(s) to sampler... ");
    allSoundsLoaded = true;
}

bool SourceSound::isSupportedAudioFileFormat(const juce::String& extension)
{
    return juce::StringArray({"ogg", "wav", "aiff", "aif", "mp3", "flac"}).contains(extension.toLowerCase().replace(".", ""));
}


bool SourceSound::fileLocationIsSupportedAudioFileFormat(juce::File location)
{
    return isSupportedAudioFileFormat(location.getFileExtension());
}

juce::File SourceSound::getFreesoundFileLocation(juce::ValueTree sourceSamplerSoundState, const GlobalContextStruct& context)
{
    juce::File locationInDisk;
    juce::String soundId = sourceSamplerSoundState.getProperty(SourceIDs::soundId);
    if (shouldUseOriginalQualityFile(sourceSamplerSoundState, context)){
        locationInDisk = context.soundsDownloadLocation.getChildFile(soundId + "-original").withFileExtension(sourceSamplerSoundState.getProperty(SourceIDs::format).toString());
    } else {
        locationInDisk = context.soundsDownloadLocation.getChildFile((juce::String)soundId).withFileExtension("ogg");
    }
    return locationInDisk;
}

bool SourceSound::shouldUseOriginalQualityFile(juce::ValueTree sourceSamplerSoundState, const GlobalContextStruct& context)
{
    // In order for a sound to be downloaded in original quality we need:
    // - an access token present
    // - the preference which indiates that original files are wanted (and the file smaller than the max size for original files)
    // - the sound sample to have the "format" metadata, othewise we don't know the file format and can't download (this can be checked with isSupportedAudioFileFormat)
    if (context.freesoundOauthAccessToken != ""){
        if (context.useOriginalFilesPreference == USE_ORIGINAL_FILES_ALWAYS){
            return isSupportedAudioFileFormat(sourceSamplerSoundState.getProperty(SourceIDs::format, ""));
        } else if (context.useOriginalFilesPreference == USE_ORIGINAL_FILES_ONLY_SHORT){
            // Only return true if sound has a filesize below the threshold
            if ((int)sourceSamplerSoundState.getProperty(SourceIDs::filesize, MAX_SIZE_FOR_ORIGINAL_FILE_DOWNLOAD + 1) <= MAX_SIZE_FOR_ORIGINAL_FILE_DOWNLOAD){
                return isSupportedAudioFileFormat(sourceSamplerSoundState.getProperty(SourceIDs::format, ""));
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
    // Message thread
    for (int i=0; i<state.getNumChildren(); i++){
        auto child = state.getChild(i);
        if (child.hasType(SourceIDs::SOUND_SAMPLE)){
            juce::File locationInDisk = getGlobalContext().sourceDataLocation.getChildFile(child.getProperty(SourceIDs::filePath, "").toString());
            if (targetFileLocation == locationInDisk){
                // Find the sample that corresponds to this download task and update state
                child.setProperty(SourceIDs::downloadProgress, percentageCompleted, nullptr);
            }
        }
    }
}

void SourceSound::downloadFinished(juce::File targetFileLocation, bool taskSucceeded)
{
    // Message thread
    bool allAlreadyDownloaded = true;
    for (int i=0; i<state.getNumChildren(); i++){
        auto child = state.getChild(i);
        if (child.hasType(SourceIDs::SOUND_SAMPLE)){
            // Find the sample that corresponds to this download task and update state
            // To consider a download as succeeded, check that the the task returned succes
            juce::File locationInDisk = getGlobalContext().sourceDataLocation.getChildFile(child.getProperty(SourceIDs::filePath, "").toString());
            if (taskSucceeded && (targetFileLocation == locationInDisk) && fileAlreadyInDisk(locationInDisk)){
                child.setProperty(SourceIDs::downloadCompleted, true, nullptr);
                child.setProperty(SourceIDs::downloadProgress, 100.0, nullptr);
            } else {
                if (!taskSucceeded && (targetFileLocation == locationInDisk)){
                    DBG("Download failed for " << locationInDisk.getFullPathName());
                    targetFileLocation.deleteFile();  // Remove partially downloaded files
                }
                if (!child.getProperty(SourceIDs::downloadCompleted, false)){
                    allAlreadyDownloaded = false;
                }
            }
        }
    }
    if (allAlreadyDownloaded){
        // If all sounds finished downloading, re-start the loading process which will now decode the downloaded files
        downloadTasks.clear();
        startLoading();
    }
}

void SourceSound::progress (juce::URL::DownloadTask *task, juce::int64 bytesDownloaded, juce::int64 totalLength)
{
    // Called from the download thread
    if (totalLength <= 0)
        return;
    float percentageCompleted = 100.0f*(float)bytesDownloaded/(float)totalLength;
    juce::File target = task->getTargetLocation();
    postToMessageThread([target, percentageCompleted](SourceSound& sound){ sound.downloadProgressUpdate(target, percentageCompleted); });
}

void SourceSound::finished(juce::URL::DownloadTask *task, bool success)
{
    // Called from the download thread
    juce::File target = task->getTargetLocation();
    bool succeeded = success && !task->hadError() && (task->statusCode() == 0 || task->statusCode() < 400);
    postToMessageThread([target, succeeded](SourceSound& sound){ sound.downloadFinished(target, succeeded); });
}
