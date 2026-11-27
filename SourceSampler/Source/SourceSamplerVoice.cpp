/*
  ==============================================================================

    SourceSamplerVoice.cpp
    Created: 23 Mar 2022 1:38:24pm
    Author:  Frederic Font Corbera

  ==============================================================================
*/

#include "SourceSamplerVoice.h"


SourceSamplerVoice::SourceSamplerVoice() {}

SourceSamplerVoice::~SourceSamplerVoice() {}

SourceSamplerSound* SourceSamplerVoice::getCurrentlyPlayingSourceSamplerSound() const noexcept {
    return static_cast<SourceSamplerSound*> (getCurrentlyPlayingSound().get());
}

void SourceSamplerVoice::setModWheelValue(int newValue)
{
    currentModWheelValue = newValue;
}

bool SourceSamplerVoice::canPlaySound (juce::SynthesiserSound* sound)
{
    return dynamic_cast<const SourceSamplerSound*> (sound) != nullptr;
}

int SourceSamplerVoice::getNoteIndex(int midiNote)
{
    /* Returns the "index" of the given MIDI note within the notes assigned to the sound being played (ignoring
     gaps in the mapping). See SourceHelpers::noteIndexInMapping. Returns 0 if no sound is being played. */
    if (auto* sound = getCurrentlyPlayingSourceSamplerSound())
        return SourceHelpers::noteIndexInMapping (sound->getMappedMidiNotes(), midiNote);
    return 0;
}

static int findNearestPositiveZeroCrossing (int position, const float* const signal, int signalLength, int maxSamplesSearch)
{
    int initialPosition = position;
    int endPosition = position + maxSamplesSearch;
    if (maxSamplesSearch < 0){
        initialPosition = position + maxSamplesSearch;  // maxSamplesSearch is negative!
        endPosition = position;
    }
    initialPosition = juce::jlimit (0, juce::jmax (0, signalLength - 2), initialPosition);
    endPosition = juce::jlimit (0, juce::jmax (0, signalLength - 2), endPosition);
    for (int i=initialPosition; i<endPosition; i++){
        if ((signal[i] < 0) && (signal[i+1] >= 0)){
            return i;
        }
    }
    return position;  // If none found, return original
}

void SourceSamplerVoice::startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound* s, int currentPitchWheelPosition)
{
    currentNoteVelocity = velocity;
    
    // This is called when note on is received
    if (auto* sound = dynamic_cast<SourceSamplerSound*> (s))
    {
        currentNoteVelocity = sound->getCorrectedVelocity(velocity); // Update velocity to use the corrected velocity (after applying velocity sensitivity correction)
        
        double pluginSampleRate = sound->pluginSampleRate;
        if (pluginSampleRate == 0.0){
            // TOOD: this is a hack to avoid this being wrongly initialized
            // I should investigate why this happens from time to time
            pluginSampleRate = 48000.0;
        }
        
        // Hold the playback data lock while reading the sound's audio data (the stretch thread might swap it)
        const juce::SpinLock::ScopedLockType playbackLock (sound->getPlaybackDataLock());
        
        // Reset processor chain internal state
        processorChain.reset();
        auto& gain = processorChain.get<masterGainIndex>();
        gain.setRampDurationSeconds(sound->pluginBlockSize/pluginSampleRate);
        
        // Reset some parameters
        adsr.reset();
        adsrFilter.reset();
        pitchModSemitones = 0.0;
        pitchBendModSemitones = 0.0;
        filterCutoffMod = 0.0;
        gainMod = 0.0;
        playheadSamplePositionMod = 0.0;
        masterPitchBendSemitones = 0.0;
        numParameterOverrides = 0;
        numMpeMappings = 0;
        
        if (mpeMode){
            // Initial per-note expression values (sent by MPE controllers on the note's channel before the note-on)
            mpeGlide = juce::jlimit (-1.0f, 1.0f, ((float) currentPitchWheelPosition - 8192.0f) / 8192.0f);
            pitchBendModSemitones = mpeGlide * (float) mpePitchBendRange;
            masterPitchBendSemitones = computePitchBendSemitones (sound, pendingMasterPitchWheelValue);
            channelPressureChanged (juce::roundToInt (mpePress * 127.0f));  // Applies the press modulations (mod2* amounts)
        }
        
        // Compute index of the currently played note
        currentlyPlayedNoteIndex = getNoteIndex(getCurrentlyPlayingNote());
        
        // Load and configure parameters from SourceSamplerSound
        adsr.setSampleRate (pluginSampleRate);
        // TODO: what should really be the sample rate below?
        adsrFilter.setSampleRate (sound->soundSampleRate/sound->pluginBlockSize); // Lower sample rate because we only update filter cutoff once per processing block...
       
        // Update the rest of parameters (that will be udpated at each block)
        updateParametersFromSourceSamplerSound(sound);
        
        // Reset processor chain state now that gain/filter targets are set, so smoothed values start at the new
        // note's values instead of ramping from the previous note played by this voice
        processorChain.reset();
        
        if (sound->gpi(SourceIDs::launchMode) == LAUNCH_MODE_FREEZE){
            // In freeze mode, playheadSamplePosition depends on the playheadPosition parameter
            playheadSamplePosition = getParam(sound, SourceIDs::playheadPosition) * sound->getLengthInSamples();
        } else {
            // Set initial playhead position according to start/end times
            if (sound->gpi(SourceIDs::reverse) == 0){
                playheadSamplePosition = startPositionSample;
                playheadDirectionIsForward = true;
            } else {
                playheadSamplePosition = endPositionSample;
                playheadDirectionIsForward = false;
            }
        }
        
        // Trigger ADSRs
        adsr.noteOn();
        adsrFilter.noteOn();
        
        publishState (sound);
    }
    else
    {
        jassertfalse; // this object can only play SourceSamplerSound!
    }
}

void SourceSamplerVoice::updateParametersFromSourceSamplerSound(SourceSamplerSound* sound)
{
    // This is called at each processing block of 64 samples
    
    // NOTE: the caller must hold sound->getPlaybackDataLock()
    updateMpeParameterOverrides(sound);
    
    if (sound->gpi(SourceIDs::launchMode) == LAUNCH_MODE_FREEZE){
        // If in freeze mode, we "only" care about the playhead position parameter, the rest of parameters to define pitch, start/end times, etc., are not relevant
        targetPlayheadSamplePosition = (getParam(sound, SourceIDs::playheadPosition) + playheadSamplePositionMod + currentModWheelValue/127.0) * sound->getLengthInSamples();
        
    } else {
        // Pitch
        int currenltlyPlayingNote = 0;
        if ((sound->gpi(SourceIDs::noteMappingMode) == NOTE_MAPPING_MODE_PITCH) || (sound->gpi(SourceIDs::noteMappingMode) == NOTE_MAPPING_MODE_BOTH)){
            currenltlyPlayingNote = getCurrentlyPlayingNote();
        } else {
            // If note mapping by pitch is not enabled, compute pitchRatio pretending the currently playing note is the same as the root note configured for that sound. In this way, pitch will not be modified depending on the played notes (but pitch bends and other modulations will still affect)
            currenltlyPlayingNote = sound->getMidiRootNote();
        }
        
        // When computing the distance to the root note, do it based on the note index and not the raw MIDI note value
        // In this waw, discontinuities in assigned notes won't have effect in pitch. This will work well in interleaved mode
        // There is an "edge" case in which a sound is assigned a contiguous region of notes to play harmonically, then
        // a region is left empty, and then a new region of assigned notes is added. That second region will play notes
        // unintuitively because the notes in the "blank" region in the middle won't be counted
        // If this behaviour becomes a problem it could be turned into a sound parameter
        int distanceToRootNote = getNoteIndex(currenltlyPlayingNote) - getNoteIndex(sound->getMidiRootNote());
        double currentNoteFrequency = std::pow (2.0, (getParam(sound, SourceIDs::pitch) + distanceToRootNote) / 12.0);
        pitchRatio = currentNoteFrequency * sound->soundSampleRate / sound->pluginSampleRate;
        
        // Set start/end and loop start/end settings
        int soundLoopStartPosition, soundLoopEndPosition;  // To be set later
        int soundLengthInSamples = sound->getLengthInSamples();
        if ((sound->gpi(SourceIDs::noteMappingMode) == NOTE_MAPPING_MODE_SLICE) || (sound->gpi(SourceIDs::noteMappingMode) == NOTE_MAPPING_MODE_BOTH)){
            // If note mapping by slice is enabled, we find the start/end positions corresponding to the current slice and set them to these
            // Also, loop start/end positions are ignored and set to the same slice start/end positions
            int globalStartPositionSample = (int)(getParam(sound, SourceIDs::startPosition) * soundLengthInSamples);
            int globalEndPositionSample = (int)(getParam(sound, SourceIDs::endPosition) * soundLengthInSamples);
            int startToEndSoundLength = globalEndPositionSample - globalStartPositionSample;
            if (sound->gpi(SourceIDs::numSlices) != SLICE_MODE_AUTO_ONSETS){
                // if not in slice by onsets mode, divide the sound in N equal slices
                int nSlices;
                if (sound->gpi(SourceIDs::numSlices) == SLICE_MODE_AUTO_NNOTES){
                    // If in auto-nnotes mode choose the number of slices automatically to be the same of the number of notes mapped to this sound
                    nSlices = sound->getNumberOfMappedMidiNotes();
                } else {
                    // Othwise, assign the selected number of slices directly
                    nSlices = sound->gpi(SourceIDs::numSlices);
                }
                nSlices = juce::jmax (1, nSlices);
                float sliceLength = (float)startToEndSoundLength / (float)nSlices;
                int currentSlice = currentlyPlayedNoteIndex % nSlices;
                startPositionSample = globalStartPositionSample + currentSlice * sliceLength;
                endPositionSample = globalStartPositionSample + (currentSlice + 1) * sliceLength;
                
            } else {
                // If in onsets mode, get the slices from the onsets analysis. Only consider those onsets inside the global start/end selection
                int globalStartPositionSample = (int)(getParam(sound, SourceIDs::startPosition) * soundLengthInSamples);
                int globalEndPositionSample = (int)(getParam(sound, SourceIDs::endPosition) * soundLengthInSamples);
                // Count onsets inside the start/end selection without allocating (this runs in the audio thread)
                const std::vector<int>& onsetTimesSamples = sound->getOnsetTimesSamples();
                int nSlices = 0;
                for (int onset: onsetTimesSamples){
                    if ((onset >= globalStartPositionSample) && (onset <= globalEndPositionSample))
                        nSlices += 1;
                }
                if (nSlices > 0){
                    // If onset data is available, use the onsets :)
                    int currentSlice = currentlyPlayedNoteIndex % nSlices;
                    int validIndex = 0;
                    startPositionSample = globalStartPositionSample;
                    endPositionSample = globalEndPositionSample;
                    for (int onset: onsetTimesSamples){
                        if ((onset < globalStartPositionSample) || (onset > globalEndPositionSample))
                            continue;
                        if (validIndex == currentSlice)
                            startPositionSample = onset;
                        else if (validIndex == currentSlice + 1){
                            endPositionSample = onset;
                            break;
                        }
                        validIndex += 1;
                    }
                } else {
                    // If no onset data is available, use all sound
                    startPositionSample = globalStartPositionSample;
                    endPositionSample = globalEndPositionSample;
                }
            }
            soundLoopStartPosition = startPositionSample;
            soundLoopEndPosition = endPositionSample;
        } else {
            // If note mapping by slice is not enabled, then all mappend notes start at the same start/end position as defined by the start/end position slider(s)
            // Also, the loop positions are defined following the sliders
            startPositionSample = (int)(getParam(sound, SourceIDs::startPosition) * soundLengthInSamples);
            endPositionSample = (int)(getParam(sound, SourceIDs::endPosition) * soundLengthInSamples);
            soundLoopStartPosition = (int)(getParam(sound, SourceIDs::loopStartPosition) * soundLengthInSamples);
            soundLoopEndPosition = (int)(getParam(sound, SourceIDs::loopEndPosition) * soundLengthInSamples);
        }
        
        // Find fixed looping points (at zero-crossings)
        if ((soundLoopStartPosition != loopStartPositionSample) || (soundLoopEndPosition != loopEndPositionSample)){
            // Either loop start or end has changed in the sound object
            auto& data = sound->getPlaybackData();
            const float* const signal = data.getReadPointer (0);  // use first audio channel to detect 0 crossing
            if (soundLoopStartPosition != loopStartPositionSample){
                // If the loop start position has changed, process it to move it to the next positive zero crossing
                fixedLoopStartPositionSample = findNearestPositiveZeroCrossing(soundLoopStartPosition, signal, data.getNumSamples(), 2000);
                //DBG("Fixed start sample position by " << fixedLoopStartPositionSample - soundLoopStartPosition);
            }
            if (soundLoopEndPosition != loopEndPositionSample){
                // If the loop end position has changed, process it to move it to the next positive zero crossing
                fixedLoopEndPositionSample = findNearestPositiveZeroCrossing(soundLoopEndPosition, signal, data.getNumSamples(), -2000);
                //DBG("Fixed end sample position by " << fixedLoopEndPositionSample - soundLoopEndPosition);
            }
        }
        loopStartPositionSample = soundLoopStartPosition;
        loopEndPositionSample = soundLoopEndPosition;

        if ((sound->gpi(SourceIDs::noteMappingMode) == NOTE_MAPPING_MODE_SLICE) || (sound->gpi(SourceIDs::noteMappingMode) == NOTE_MAPPING_MODE_BOTH)){
            // If in some slicing mode, because start/end position and loop start/end positions are the same, now that we have "fixed" the
            // loop start/end position to the nearest zero crossing, do the same for the slice start/end so we avoid clicks at start and
            // end of slices
            startPositionSample = loopStartPositionSample;
            endPositionSample = loopEndPositionSample;
        }
    }
    
    // ADSRs
    juce::ADSR::Parameters ampADSRParams = {getParam(sound, SourceIDs::attack), getParam(sound, SourceIDs::decay), getParam(sound, SourceIDs::sustain), getParam(sound, SourceIDs::release)};
    adsr.setParameters (ampADSRParams);
    juce::ADSR::Parameters filterADSRParams = {getParam(sound, SourceIDs::filterAttack), getParam(sound, SourceIDs::filterDecay), getParam(sound, SourceIDs::filterSustain), getParam(sound, SourceIDs::filterRelease)};
    adsrFilter.setParameters (filterADSRParams);
    
    // Filter
    
    // Compute velocity modulations (only relevant at start of note)
    filterCutoff = getParam(sound, SourceIDs::filterCutoff); // * std::pow(2, getCurrentlyPlayingNote() - sound->midiRootNote) * sound->filterKeyboardTracking;  // Add kb tracking
    filterRessonance = getParam(sound, SourceIDs::filterRessonance);
    float filterCutoffVelMod = currentNoteVelocity * getParam(sound, SourceIDs::filterCutoff) * getParam(sound, SourceIDs::vel2CutoffAmt);
    float newFilterCutoffMod = filterCutoffMod + (currentModWheelValue/127.0) * filterCutoff * getParam(sound, SourceIDs::mod2CutoffAmt);  // Add mod wheel modulation and aftertouch here
    float filterADSRMod = adsrFilter.getNextSample() * filterCutoff * getParam(sound, SourceIDs::filterADSR2CutoffAmt);
    float computedCutoff = (1.0 - getParam(sound, SourceIDs::filterKeyboardTracking)) * filterCutoff + getParam(sound, SourceIDs::filterKeyboardTracking) * filterCutoff * std::pow(2.0f, (float)(getCurrentlyPlayingNote() - sound->getMidiRootNote()) / 12.0f) + // Base cutoff and kb tracking
                           filterCutoffVelMod + // Velocity mod to cutoff
                           newFilterCutoffMod +  // Aftertouch mod/modulation wheel mod
                           filterADSRMod; // ADSR mod
    auto& filter = processorChain.get<filterIndex>();
    filter.setCutoffFrequencyHz (juce::jmax(0.001f, computedCutoff));
    filter.setResonance (filterRessonance);
    
    // Amp and pan
    float velocityGain = (getParam(sound, SourceIDs::vel2GainAmt) * currentNoteVelocity) + (1 - getParam(sound, SourceIDs::vel2GainAmt));
    lgain = velocityGain;
    rgain = velocityGain;
    pan = getParam(sound, SourceIDs::pan);
    auto& gain = processorChain.get<masterGainIndex>();
    float newGainMod;
    if (getParam(sound, SourceIDs::mod2GainAmt) >= 0){  // Set a maximum gain modulation combining mod wheel and aftertouch
        newGainMod = (float)juce::jmin((double)(gainMod + getParam(sound, SourceIDs::mod2GainAmt) * (double)currentModWheelValue/127.0), (double)getParam(sound, SourceIDs::mod2GainAmt));  // Add mod wheel modulation here
    } else {
        newGainMod = (float)juce::jmax((double)(gainMod + getParam(sound, SourceIDs::mod2GainAmt) * (double)currentModWheelValue/127.0), (double)getParam(sound, SourceIDs::mod2GainAmt));  // Add mod wheel modulation here
    }
    gain.setGainDecibels(getParam(sound, SourceIDs::gain) + newGainMod);
}

void SourceSamplerVoice::stopNote (float /*velocity*/, bool allowTailOff)
{
    if (allowTailOff) {
        // This is the case when receiving a note off event
        if (auto* sound = getCurrentlyPlayingSourceSamplerSound()){
            if (sound->gpi(SourceIDs::launchMode) == LAUNCH_MODE_TRIGGER){
                // We only trigger ADSRs release phase if we're in gate or loop launch modes, otherwise continue playing normally
            } else {
                adsr.noteOff();
                adsrFilter.noteOff();
            }
        }
    } else {
        // This is the case when we reached the end of the sound (or the end of the release stage) or for some other reason we want to cut abruptly
        publishState (nullptr);
        clearCurrentNote();
    }
}

double SourceSamplerVoice::computePitchBendSemitones (SourceSamplerSound* sound, int pitchWheelValue)
{
    // 8192 = middle value, 16383 = max, 0 = min. Uses the sound's pitch bend ranges.
    if (pitchWheelValue >= 8192)
        return (double)(((float)pitchWheelValue - 8192.0f)/8192.0f) * getParam(sound, SourceIDs::pitchBendRangeUp);
    return (double)((8192.0f - (float)pitchWheelValue)/8192.0f) * getParam(sound, SourceIDs::pitchBendRangeDown) * -1;
}

void SourceSamplerVoice::pitchWheelMoved (int newValue) {
    if (auto* sound = getCurrentlyPlayingSourceSamplerSound()){
        if (mpeMode){
            // MPE per-note pitch bend ("glide") uses the global MPE pitch bend range
            mpeGlide = juce::jlimit (-1.0f, 1.0f, ((float) newValue - 8192.0f) / 8192.0f);
            pitchBendModSemitones = mpeGlide * (float) mpePitchBendRange;
        } else {
            pitchBendModSemitones = computePitchBendSemitones (sound, newValue);
        }
    }
}

void SourceSamplerVoice::setMasterPitchBend (int pitchWheelValue)
{
    pendingMasterPitchWheelValue = pitchWheelValue;
    if (auto* sound = getCurrentlyPlayingSourceSamplerSound())
        masterPitchBendSemitones = computePitchBendSemitones (sound, pitchWheelValue);
}

void SourceSamplerVoice::setMpeState (bool enabled, int perNotePitchBendRange, int initialPressure, int initialSlide, int masterPitchWheelValue)
{
    mpeMode = enabled;
    mpePitchBendRange = perNotePitchBendRange;
    mpePress = juce::jlimit (0.0f, 1.0f, (float) initialPressure / 127.0f);
    mpeSlide = juce::jlimit (0.0f, 1.0f, (float) initialSlide / 127.0f);
    mpeGlide = 0.0f;
    pendingMasterPitchWheelValue = enabled ? masterPitchWheelValue : 8192;
}

float SourceSamplerVoice::getMpeSourceValue (SourceHelpers::MpeSource source) const noexcept
{
    switch (source)
    {
        case SourceHelpers::MpeSource::slide:  return mpeSlide;
        case SourceHelpers::MpeSource::glide:  return (mpeGlide + 1.0f) * 0.5f;
        case SourceHelpers::MpeSource::strike: return currentNoteVelocity;
        case SourceHelpers::MpeSource::numSources: break;
    }
    return 0.0f;
}

void SourceSamplerVoice::updateMpeParameterOverrides (SourceSamplerSound* sound)
{
    // Computes the parameter values set by the sound's MPE mappings for this voice (audio thread, no allocations)
    numParameterOverrides = 0;
    if (!mpeMode)
        return;
    const int numCopied = sound->getSourceSound()->copyMpeMappings (mpeMappings);
    if (numCopied >= 0)
        numMpeMappings = numCopied;  // Otherwise (mappings being edited) keep using the previous copy
    for (int i = 0; i < numMpeMappings; ++i)
    {
        const auto& mapping = mpeMappings[(size_t) i];
        const float value = SourceHelpers::mapMpeValueToParameter (getMpeSourceValue (mapping.source), mapping.curve,
                                                                   SourceHelpers::isBipolarMpeSource (mapping.source),
                                                                   mapping.minRange, mapping.maxRange,
                                                                   mapping.parameterMin, mapping.parameterMax);
        // If several mappings target the same parameter, the last one wins
        int index = 0;
        while (index < numParameterOverrides && parameterOverrides[(size_t) index].first != mapping.parameter)
            ++index;
        parameterOverrides[(size_t) index] = { mapping.parameter, value };
        numParameterOverrides = juce::jmax (numParameterOverrides, index + 1);
    }
}

void SourceSamplerVoice::aftertouchChanged(int newAftertouchValue)
{
    if (auto* sound = getCurrentlyPlayingSourceSamplerSound())
    {
        pitchModSemitones = getParam(sound, SourceIDs::mod2PitchAmt) * (double)newAftertouchValue/127.0;
        filterCutoffMod = (newAftertouchValue/127.0) * filterCutoff * getParam(sound, SourceIDs::mod2CutoffAmt);
        gainMod = getParam(sound, SourceIDs::mod2GainAmt) * (float)newAftertouchValue/127.0;
        playheadSamplePositionMod = (double)newAftertouchValue/127.0;
    }
}


void SourceSamplerVoice::channelPressureChanged  (int newChannelPressureValue)
{
    mpePress = juce::jlimit (0.0f, 1.0f, (float) newChannelPressureValue / 127.0f);
    if (auto* sound = getCurrentlyPlayingSourceSamplerSound())
    {
        pitchModSemitones = getParam(sound, SourceIDs::mod2PitchAmt) * (double)newChannelPressureValue/127.0;
        filterCutoffMod = (newChannelPressureValue/127.0) * filterCutoff * getParam(sound, SourceIDs::mod2CutoffAmt);
        gainMod = getParam(sound, SourceIDs::mod2GainAmt) * (float)newChannelPressureValue/127.0;
        playheadSamplePositionMod = (double)newChannelPressureValue/127.0;
    }
}

void SourceSamplerVoice::controllerMoved (int controllerNumber, int newValue)
{
    // Set new modulation wheel value
    if (controllerNumber == 1){
        setModWheelValue(newValue);
    } else if (controllerNumber == MPE_SLIDE_CC){
        mpeSlide = juce::jlimit (0.0f, 1.0f, (float) newValue / 127.0f);
    }
}

static inline float interpolateSample (double samplePosition, const float* const signal, int signalLength)
{
    // Linear interpolation, returns 0 outside the signal bounds
    if (samplePosition < 0.0 || signalLength <= 0)
        return 0.0f;
    auto pos = (int) samplePosition;
    if (pos >= signalLength)
        return 0.0f;
    auto alpha = (float) (samplePosition - pos);
    auto next = pos + 1 < signalLength ? signal[pos + 1] : 0.0f;
    return signal[pos] * (1.0f - alpha) + next * alpha;
}

//==============================================================================
void SourceSamplerVoice::renderNextBlock (juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples)
{
    if (auto* sound = getCurrentlyPlayingSourceSamplerSound())
    {
        // Hold the playback data lock while rendering (the stretch thread might swap the buffer otherwise)
        const juce::SpinLock::ScopedLockType playbackLock (sound->getPlaybackDataLock());
        
        // Do some preparation (not all parameters will be used depending on the launch mode)
        int originalNumSamples = numSamples; // user later for filter processing
        double previousPitchRatio = pitchRatio;
        float previousPitchModSemitones = (float)juce::jmin((double)pitchModSemitones + getParam(sound, SourceIDs::mod2PitchAmt) * (double)currentModWheelValue/127.0, (double)getParam(sound, SourceIDs::mod2PitchAmt));  // Add mod wheel position
        float previousPitchBendModSemitones = (float) (pitchBendModSemitones + masterPitchBendSemitones);
        float previousPan = pan;
        bool noteStoppedHard = false;
        updateParametersFromSourceSamplerSound(sound);
        
        // Sampler reading and rendering
        auto& data = sound->getPlaybackData();
        const int dataLength = data.getNumSamples();
        const float* const inL = data.getReadPointer (0);
        const float* const inR = data.getNumChannels() > 1 ? data.getReadPointer (1) : nullptr;
        
        // Parameters that don't change within a block
        const int launchMode = sound->gpi(SourceIDs::launchMode);
        const int loopXFadeNSamples = sound->gpi(SourceIDs::loopXFadeNSamples);
        
        // The playback buffer might have changed length (e.g. after re-computing time stretch), make sure the playhead is in range
        playheadSamplePosition = juce::jlimit (0.0, (double) juce::jmax (0, dataLength - 1), playheadSamplePosition);
        
        tmpVoiceBuffer.clear();
        float* outL = tmpVoiceBuffer.getWritePointer (0, 0);
        float* outR = tmpVoiceBuffer.getNumChannels() > 1 ? tmpVoiceBuffer.getWritePointer (1, 0) : nullptr;
        
        while (--numSamples >= 0)
        {
            // Calculate L and R samples using basic interpolation
            float l = interpolateSample(playheadSamplePosition, inL, dataLength);
            float r = (inR != nullptr) ? interpolateSample(playheadSamplePosition, inR, dataLength) : l;
            
            if (launchMode != LAUNCH_MODE_FREEZE){
                // Outside freeze mode, add samples from the source sound to the buffer and check for looping and other sorts of modulations
                
                // Check, in case we're looping, if we are in a crossfade zone and should do crossfade
                if ((launchMode == LAUNCH_MODE_LOOP) && loopXFadeNSamples > 0){
                    // NOTE: don't crossfade in LAUNCH_MODE_LOOP_FW_BW mode because it loops from the the same sample (no need to crossfade)
                    if (playheadDirectionIsForward){
                        // PLayhead going forward  (normal playing mode): do loop when reahing fixedLoopEndPositionSample
                        float samplesToLoopEndPositionSample = (float)fixedLoopEndPositionSample - playheadSamplePosition;
                        if ((samplesToLoopEndPositionSample > 0) && (samplesToLoopEndPositionSample < loopXFadeNSamples)){
                            if (ENABLE_DEBUG_BUFFER == 1){
                                startRecordingToDebugBuffer(loopXFadeNSamples * 2);
                            }
                            
                            // We are approaching loopEndPositionSample and are closer than sound->loopXFadeNSamples
                            float lcrossfadeSample = 0.0;
                            float rcrossfadeSample = 0.0;
                            float crossfadeGain = 0.0;
                            float crossfadePos = (float)fixedLoopStartPositionSample - samplesToLoopEndPositionSample;
                            if (crossfadePos > 0){
                                lcrossfadeSample = interpolateSample(crossfadePos, inL, dataLength);
                                rcrossfadeSample = (inR != nullptr) ? interpolateSample(crossfadePos, inR, dataLength) : lcrossfadeSample;
                                crossfadeGain = (float)samplesToLoopEndPositionSample/loopXFadeNSamples;
                            } else {
                                // If position is negative, there is no data to do the crossfade
                            }
                            l = l * (crossfadeGain) + lcrossfadeSample * (1.0f - crossfadeGain);
                            r = r * (crossfadeGain) + rcrossfadeSample * (1.0f - crossfadeGain);
                        } else {
                            // Do nothing because we're not in crossfade zone
                        }
                    } else {
                        // Playhead going backwards: do loop when reahing fixedLoopEndPositionSample
                        int samplesToLoopStartPositionSample = playheadSamplePosition - (float)fixedLoopStartPositionSample;
                        if ((samplesToLoopStartPositionSample > 0) && (samplesToLoopStartPositionSample < loopXFadeNSamples)){
                            // We are approaching loopStartPositionSample (going backwards) and are closer than sound->loopXFadeNSamples
                            float lcrossfadeSample = 0.0;
                            float rcrossfadeSample = 0.0;
                            float crossfadeGain = 0.0;
                            float crossfadePos = (float)fixedLoopEndPositionSample + samplesToLoopStartPositionSample;
                            if (crossfadePos < dataLength){
                                lcrossfadeSample = interpolateSample(crossfadePos, inL, dataLength);
                                rcrossfadeSample = (inR != nullptr) ? interpolateSample(crossfadePos, inR, dataLength) : lcrossfadeSample;
                                crossfadeGain = (float)samplesToLoopStartPositionSample/loopXFadeNSamples;
                            } else {
                                // If position is above playing sound length, there is no data to do the crossfade
                            }
                            l = l * (crossfadeGain) + lcrossfadeSample * (1.0f - crossfadeGain);
                            r = r * (crossfadeGain) + rcrossfadeSample * (1.0f - crossfadeGain);
                        } else {
                            // Do nothing because we're not in crossfade zone
                        }
                    }
                }
            }
            
            if (ENABLE_DEBUG_BUFFER == 1){
                writeToDebugBuffer(l);
            }
            
            // Draw envelope sample and add it to L and R samples, also add panning and velocity gain
            float rGainPan = juce::jmin (1 - pan, 1.0f);
            float lGainPan = juce::jmin (1 + pan, 1.0f);
            float previousRGainPan = juce::jmin (1 - previousPan, 1.0f);
            float previousLGainPan = juce::jmin (1 + previousPan, 1.0f);
            float interpolatedRGainPan = (previousRGainPan * ((float)numSamples/originalNumSamples) + rGainPan * (1.0f - (float)numSamples/originalNumSamples));
            float interpolatedLGainPan = (previousLGainPan * ((float)numSamples/originalNumSamples) + lGainPan * (1.0f - (float)numSamples/originalNumSamples));
            auto envelopeValue = adsr.getNextSample();
            
            l *= lgain * interpolatedLGainPan * envelopeValue;
            r *= rgain * interpolatedRGainPan * envelopeValue;

            // Update output buffer with L and R samples
            if (outR != nullptr) {
                *outL++ += l;
                *outR++ += r;
            } else {
                *outL++ += (l + r) * 0.5f;
            }

            if (launchMode == LAUNCH_MODE_FREEZE){
                // If in freeze mode, move from the current playhead position to the target playhead position in the length of the block
                double distanceTotargetPlayheadSamplePosition = targetPlayheadSamplePosition - playheadSamplePosition;
                double distanceTotargetPlayheadSamplePositionNormalized = std::abs(distanceTotargetPlayheadSamplePosition / sound->getLengthInSamples()); // normalized between 0 and 1
                double maxSpeed = juce::jmax(std::pow(distanceTotargetPlayheadSamplePositionNormalized, 2) * getParam(sound, SourceIDs::freezePlayheadSpeed), 1.0);
                double actualSpeed = juce::jmin(maxSpeed, std::abs(distanceTotargetPlayheadSamplePosition));
                if (distanceTotargetPlayheadSamplePosition >= 0){
                    playheadSamplePosition += actualSpeed;
                } else {
                    playheadSamplePosition -= actualSpeed;
                }
                playheadSamplePosition = juce::jlimit(0.0, (double)juce::jmax(0, dataLength - 1), playheadSamplePosition);  // Just to be sure...
 
            } else {
                // If not in freeze mode, advance source sample position for next iteration according to pitch ratio and other modulations...
                double interpolatedPitchRatio = (previousPitchRatio * ((double)numSamples/originalNumSamples) + pitchRatio * (1.0f - (double)numSamples/originalNumSamples));
                float interpolatedPitchModSemitones = (previousPitchModSemitones * ((float)numSamples/originalNumSamples) + pitchModSemitones * (1.0f - (float)numSamples/originalNumSamples));
                float interpolatedPitchBendModSemitones = (previousPitchBendModSemitones * ((float)numSamples/originalNumSamples) + (float)(pitchBendModSemitones + masterPitchBendSemitones) * (1.0f - (float)numSamples/originalNumSamples));
                if (playheadDirectionIsForward){
                    playheadSamplePosition += interpolatedPitchRatio * std::exp2((interpolatedPitchModSemitones + interpolatedPitchBendModSemitones) / 12.0);
                } else {
                    playheadSamplePosition -= interpolatedPitchRatio * std::exp2((interpolatedPitchModSemitones + interpolatedPitchBendModSemitones) / 12.0);
                }
                
                // ... also check if we're reaching the end of the sound or looping region to do looping
                if ((launchMode == LAUNCH_MODE_LOOP) || (launchMode == LAUNCH_MODE_LOOP_FW_BW)){
                    // If looping is enabled, check whether we should loop
                    if (playheadDirectionIsForward){
                        if (playheadSamplePosition > fixedLoopEndPositionSample){
                            if (launchMode == LAUNCH_MODE_LOOP_FW_BW) {
                                // Forward<>Backward loop mode (ping pong): stay on loop end but change direction
                                playheadDirectionIsForward = !playheadDirectionIsForward;
                            } else {
                                // Foward loop mode: jump from loop end to loop start
                                playheadSamplePosition = fixedLoopStartPositionSample;
                            }
                        }
                    } else {
                        if (playheadSamplePosition < fixedLoopStartPositionSample){
                            if (launchMode == LAUNCH_MODE_LOOP_FW_BW) {
                                // Forward<>Backward loop mode (ping pong): stay on loop end but change direction
                                playheadDirectionIsForward = !playheadDirectionIsForward;
                            } else {
                                // Forward loop mode (in reverse): jump from loop start to loop end
                                playheadSamplePosition = fixedLoopEndPositionSample;
                            }
                        }
                    }
                } else {
                    // If not looping, check whether we've reached the end of the file
                    bool endReached = false;
                    if (playheadDirectionIsForward){
                        if (playheadSamplePosition > endPositionSample){
                            endReached = true;
                        }
                    } else {
                        if (playheadSamplePosition < startPositionSample){
                            endReached = true;
                        }
                    }
                    
                    if (endReached)
                    {
                        stopNote (0.0f, false);
                        noteStoppedHard = true;
                        break;
                    }
                }
            }
            
            if (!adsr.isActive() && !noteStoppedHard){
                stopNote (0.0f, false);
            }
        }
        

        // Apply filter
        auto block = juce::dsp::AudioBlock<float> (tmpVoiceBuffer);
        auto blockToUse = block.getSubBlock (0, (size_t) originalNumSamples);
        auto contextToUse = juce::dsp::ProcessContextReplacing<float> (blockToUse);
        processorChain.process (contextToUse);
        
        // Add voice signal (including filter) to the output buffer
        juce::dsp::AudioBlock<float> (outputBuffer)
            .getSubBlock ((size_t) startSample, (size_t) originalNumSamples)
            .add (blockToUse);
        
        // Publish the new playhead position for the UI (unless the note finished in this block)
        if (getCurrentlyPlayingSound() != nullptr)
            publishState (sound);
    }
}

void SourceSamplerVoice::prepare (const juce::dsp::ProcessSpec& spec)
{
    tmpVoiceBuffer = juce::AudioBuffer<float>(spec.numChannels, spec.maximumBlockSize);
    processorChain.prepare (spec);
}

void SourceSamplerVoice::publishState (SourceSamplerSound* playingSound) noexcept
{
    if (playingSound == nullptr){
        publishedActive.store (false, std::memory_order_release);
        publishedPlayPosition.store (-1.0f, std::memory_order_relaxed);
        return;
    }
    publishedSourceSound.store (playingSound->getSourceSound(), std::memory_order_relaxed);
    const int length = playingSound->getLengthInSamples();
    publishedPlayPosition.store (length > 0 ? (float) (playheadSamplePosition / length) : 0.0f, std::memory_order_relaxed);
    publishedActive.store (true, std::memory_order_release);
}

SourceSamplerVoice::PublishedState SourceSamplerVoice::getPublishedState() const noexcept
{
    PublishedState state;
    state.active = publishedActive.load (std::memory_order_acquire);
    if (state.active){
        state.sourceSound = publishedSourceSound.load (std::memory_order_relaxed);
        state.playPosition = publishedPlayPosition.load (std::memory_order_relaxed);
    }
    return state;
}

float SourceSamplerVoice::getPlayingPositionPercentage()
{
    if (auto* sound = getCurrentlyPlayingSourceSamplerSound())
    {
        return (float)playheadSamplePosition/(float)sound->getLengthInSamples();
    }
    return -1.0;
}


void SourceSamplerVoice::startRecordingToDebugBuffer(int bufferSize){
    if (!isRecordingToDebugBuffer && !debugBufferFinishedRecording){
        //DBG("Started writing to buffer...");
        debugBuffer = juce::AudioBuffer<float>(1, bufferSize);
        isRecordingToDebugBuffer = true;
    }
}

void SourceSamplerVoice::writeToDebugBuffer(float sample){
    if (isRecordingToDebugBuffer){
        auto ptr = debugBuffer.getWritePointer(0);
        ptr[debugBufferCurrentPosition] = sample;
        debugBufferCurrentPosition += 1;
        if (debugBufferCurrentPosition >= debugBuffer.getNumSamples()){
            endRecordingToDebugBuffer("debugRecording");
        }
    }
}

void SourceSamplerVoice::endRecordingToDebugBuffer(juce::String outFilename){
    if (isRecordingToDebugBuffer){
        isRecordingToDebugBuffer = false;
        debugBufferCurrentPosition = 0;
        juce::WavAudioFormat format;
        std::unique_ptr<juce::AudioFormatWriter> writer;
        juce::File file = juce::File::getSpecialLocation(juce::File::userDesktopDirectory).getChildFile(outFilename).withFileExtension("wav");
        writer.reset (format.createWriterFor (new juce::FileOutputStream (file),
                                              getSampleRate(),
                                              debugBuffer.getNumChannels(),
                                              24,
                                              {},
                                              0));
        if (writer != nullptr){
            writer->writeFromAudioSampleBuffer (debugBuffer, 0, debugBuffer.getNumSamples());
            debugBufferFinishedRecording = true;
            //DBG("Writing buffer to file...");
        }
    }
}

