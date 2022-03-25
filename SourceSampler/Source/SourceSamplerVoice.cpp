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

bool SourceSamplerVoice::canPlaySound (SynthesiserSound* sound)
{
    return dynamic_cast<const SourceSamplerSound*> (sound) != nullptr;
}

int SourceSamplerVoice::getNoteIndex(int midiNote)
{
    /* This function compares the MIDI note passed as parameter and the list of midi notes assigned to the
     sound and returns the "index" of the note passed as parameter. For example, if the first MIDI note
     assigned to the sound is C3 and the passed note is also C3, this function returns 0. If sound
     is assigned notes C3, C#3 and D3 and the passed note is C5, this function returns 2. If no notes
     are assigned to the sound or no sound is being played, this function returns 0 (this function will not be
     called in these cases anyway). If there are note discontinuities in the mapping, the discontinuities are
     not counted (e.g. if a sound is assigned notes [C3, C#3, E3] and the note being played is E3, this function
     will return 2. If the note being passed is not one of the assigned notes, returns the closest one.
          */
    if (auto* sound = getCurrentlyPlayingSourceSamplerSound())
    {
        if (sound->getNumberOfMappedMidiNotes() > 0){
            BigInteger mappedMidiNotes = sound->getMappedMidiNotes();
            int noteIndex = 0;
            int closestNoteIndex = 0;
            int minDistanceWithNote = 1000;
            int i = 0;
            while (true) {
                int nextMappedNote = mappedMidiNotes.findNextSetBit(i);
                if (nextMappedNote == midiNote){
                    return noteIndex;
                } else if (nextMappedNote == -1){
                    break;
                }
                noteIndex += 1;
                int distanceWithNote = std::abs(noteIndex - midiNote);
                if (distanceWithNote <= minDistanceWithNote){
                    closestNoteIndex = noteIndex;
                    minDistanceWithNote = distanceWithNote;
                }
                i = nextMappedNote + 1;
            }
            return closestNoteIndex;
        }
    }
    return 0;
}

int findNearestPositiveZeroCrossing (int position, const float* const signal, int maxSamplesSearch)
{
    int initialPosition = position;
    int endPosition = position + maxSamplesSearch;
    if (maxSamplesSearch < 0){
        initialPosition = position + maxSamplesSearch;  // maxSamplesSearch is negative!
        endPosition = position;
    }
    for (int i=initialPosition; i<endPosition; i++){
        if ((signal[i] < 0) && (signal[i+1] >= 0)){
            return i;
        }
    }
    return position;  // If none found, return original
}

void SourceSamplerVoice::startNote (int midiNoteNumber, float velocity, SynthesiserSound* s, int /*currentPitchWheelPosition*/)
{
    // This is called when note on is received
    if (auto* sound = dynamic_cast<SourceSamplerSound*> (s))
    {
        double pluginSampleRate = sound->pluginSampleRate;
        if (pluginSampleRate == 0.0){
            // TOOD: this is a hack to avoid this being wrongly initialized
            // I should investigate why this happens from time to time
            pluginSampleRate = 48000.0;
        }
        
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
        
        // Compute index of the currently played note
        currentlyPlayedNoteIndex = getNoteIndex(getCurrentlyPlayingNote());
        
        // Load and configure parameters from SourceSamplerSound
        adsr.setSampleRate (pluginSampleRate);
        // TODO: what should really be the sample rate below?
        adsrFilter.setSampleRate (sound->soundSampleRate/sound->pluginBlockSize); // Lower sample rate because we only update filter cutoff once per processing block...
        
        // Compute velocity modulations (only relevant at start of note)
        filterCutoffVelMod = velocity * sound->filterCutoff * sound->vel2CutoffAmt;
        float velocityGain = (sound->vel2GainAmt * velocity) + (1 - sound->vel2GainAmt);
        lgain = velocityGain;
        rgain = velocityGain;
        
        // Update the rest of parameters (that will be udpated at each block)
        updateParametersFromSourceSamplerSound(sound);
        
        if (sound->getParameterInt("launchMode") == LAUNCH_MODE_FREEZE){
            // In freeze mode, playheadSamplePosition depends on the playheadPosition parameter
            playheadSamplePosition = sound->playheadPosition * sound->getLengthInSamples();
        } else {
            // Set initial playhead position according to start/end times
            if (sound->reverse == 0){
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
    }
    else
    {
        jassertfalse; // this object can only play SourceSamplerSound!
    }
}

void SourceSamplerVoice::updateParametersFromSourceSamplerSound(SourceSamplerSound* sound)
{
    // This is called at each processing block of 64 samples
    
    if (sound->getParameterInt("launchMode") == LAUNCH_MODE_FREEZE){
        // If in freeze mode, we "only" care about the playhead position parameter, the rest of parameters to define pitch, start/end times, etc., are not relevant
        targetPlayheadSamplePosition = (sound->playheadPosition + playheadSamplePositionMod + currentModWheelValue/127.0) * sound->getLengthInSamples();
        
    } else {
        // Pitch
        int currenltlyPlayingNote = 0;
        if ((sound->noteMappingMode == NOTE_MAPPING_MODE_PITCH) || (sound->noteMappingMode == NOTE_MAPPING_MODE_BOTH)){
            currenltlyPlayingNote = getCurrentlyPlayingNote();
        } else {
            // If note mapping by pitch is not enabled, compute pitchRatio pretending the currently playing note is the same as the root note configured for that sound. In this way, pitch will not be modified depending on the played notes (but pitch bends and other modulations will still affect)
            currenltlyPlayingNote = sound->midiRootNote;
        }
        
        // When computing the distance to the root note, do it based on the note index and not the raw MIDI note value
        // In this waw, discontinuities in assigned notes won't have effect in pitch. This will work well in interleaved mode
        // There is an "edge" case in which a sound is assigned a contiguous region of notes to play harmonically, then
        // a region is left empty, and then a new region of assigned notes is added. That second region will play notes
        // unintuitively because the notes in the "blank" region in the middle won't be counted
        // If this behaviour becomes a problem it could be turned into a sound parameter
        int distanceToRootNote = getNoteIndex(currenltlyPlayingNote) - getNoteIndex(sound->midiRootNote);
        double currentNoteFrequency = std::pow (2.0, (sound->getParameterFloat("pitch") + distanceToRootNote) / 12.0);
        pitchRatio = currentNoteFrequency * sound->soundSampleRate / getSampleRate();
        
        // Set start/end and loop start/end settings
        int soundLoopStartPosition, soundLoopEndPosition;  // To be set later
        int soundLengthInSamples = sound->getLengthInSamples();
        if ((sound->noteMappingMode == NOTE_MAPPING_MODE_SLICE) || (sound->noteMappingMode == NOTE_MAPPING_MODE_BOTH)){
            // If note mapping by slice is enabled, we find the start/end positions corresponding to the current slice and set them to these
            // Also, loop start/end positions are ignored and set to the same slice start/end positions
            int globalStartPositionSample = (int)(sound->getParameterFloat("startPosition") * soundLengthInSamples);
            int globalEndPositionSample = (int)(sound->getParameterFloat("endPosition") * soundLengthInSamples);
            int startToEndSoundLength = globalEndPositionSample - globalStartPositionSample;
            if (sound->numSlices != SLICE_MODE_AUTO_ONSETS){
                // if not in slice by onsets mode, divide the sound in N equal slices
                int nSlices;
                if (sound->numSlices == SLICE_MODE_AUTO_NNOTES){
                    // If in auto-nnotes mode choose the number of slices automatically to be the same of the number of notes mapped to this sound
                    nSlices = sound->getNumberOfMappedMidiNotes();
                } else {
                    // Othwise, assign the selected number of slices directly
                    nSlices = sound->numSlices;
                }
                float sliceLength = startToEndSoundLength / nSlices;
                int currentSlice = currentlyPlayedNoteIndex % nSlices;
                startPositionSample = globalStartPositionSample + currentSlice * sliceLength;
                endPositionSample = globalStartPositionSample + (currentSlice + 1) * sliceLength;
                
            } else {
                // If in onsets mode, get the slices from the onsets analysis. Only consider those onsets inside the global start/end selection
                int globalStartPositionSample = (int)(sound->getParameterFloat("startPosition") * soundLengthInSamples);
                int globalEndPositionSample = (int)(sound->getParameterFloat("endPosition") * soundLengthInSamples);
                std::vector<int> onsetTimesSamples = sound->getOnsetTimesSamples();
                if (onsetTimesSamples.size() > 0){
                    // If onset data is available, use the onsets :)
                    std::vector<int> validOnsetTimesSamples = {};
                    for (int i=0; i<onsetTimesSamples.size(); i++){
                        if ((onsetTimesSamples[i] >= globalStartPositionSample) && (onsetTimesSamples[i] <= globalEndPositionSample)){
                            validOnsetTimesSamples.push_back(onsetTimesSamples[i]);
                        }
                    }
                    int nSlices = (int)validOnsetTimesSamples.size();
                    int currentSlice = currentlyPlayedNoteIndex % nSlices;
                    startPositionSample = validOnsetTimesSamples[currentSlice];
                    if (currentSlice < nSlices - 1){
                        endPositionSample = validOnsetTimesSamples[currentSlice + 1];
                    } else {
                        endPositionSample = globalEndPositionSample;
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
            startPositionSample = (int)(sound->getParameterFloat("startPosition") * soundLengthInSamples);
            endPositionSample = (int)(sound->getParameterFloat("endPosition") * soundLengthInSamples);
            soundLoopStartPosition = (int)(sound->loopStartPosition * soundLengthInSamples);
            soundLoopEndPosition = (int)(sound->loopEndPosition * soundLengthInSamples);
        }
        
        // Find fixed looping points (at zero-crossings)
        if ((soundLoopStartPosition != loopStartPositionSample) || (soundLoopEndPosition != loopEndPositionSample)){
            // Either loop start or end has changed in the sound object
            auto& data = *sound->data;
            const float* const signal = data.getReadPointer (0);  // use first audio channel to detect 0 crossing
            if (soundLoopStartPosition != loopStartPositionSample){
                // If the loop start position has changed, process it to move it to the next positive zero crossing
                fixedLoopStartPositionSample = findNearestPositiveZeroCrossing(soundLoopStartPosition, signal, 2000);
                //DBG("Fixed start sample position by " << fixedLoopStartPositionSample - soundLoopStartPosition);
            }
            if (soundLoopEndPosition != loopEndPositionSample){
                // If the loop end position has changed, process it to move it to the next positive zero crossing
                fixedLoopEndPositionSample = findNearestPositiveZeroCrossing(soundLoopEndPosition, signal, -2000);
                //DBG("Fixed end sample position by " << fixedLoopEndPositionSample - soundLoopEndPosition);
            }
        }
        loopStartPositionSample = soundLoopStartPosition;
        loopEndPositionSample = soundLoopEndPosition;
        
        if ((sound->noteMappingMode == NOTE_MAPPING_MODE_SLICE) || (sound->noteMappingMode == NOTE_MAPPING_MODE_BOTH)){
            // If in some slicing mode, because start/end position and loop start/end positions are the same, now that we have "fixed" the
            // loop start/end position to the nearest zero crossing, do the same for the slice start/end so we avoid clicks at start and
            // end of slices
            startPositionSample = loopStartPositionSample;
            endPositionSample = loopEndPositionSample;
        }
    }
    
    // ADSRs
    adsr.setParameters (sound->ampADSR);
    adsrFilter.setParameters (sound->filterADSR);
    
    // Filter
    filterCutoff = sound->filterCutoff; // * std::pow(2, getCurrentlyPlayingNote() - sound->midiRootNote) * sound->filterKeyboardTracking;  // Add kb tracking
    filterRessonance = sound->filterRessonance;
    float newFilterCutoffMod = filterCutoffMod + (currentModWheelValue/127.0) * filterCutoff * sound->mod2CutoffAmt;  // Add mod wheel modulation and aftertouch here
    float filterADSRMod = adsrFilter.getNextSample() * filterCutoff * sound->filterADSR2CutoffAmt;
    auto& filter = processorChain.get<filterIndex>();
    float computedCutoff = (1.0 - sound->filterKeyboardTracking) * filterCutoff + sound->filterKeyboardTracking * filterCutoff * std::pow(2, (getCurrentlyPlayingNote() - sound->midiRootNote)/12) + // Base cutoff and kb tracking
                           filterCutoffVelMod + // Velocity mod to cutoff
                           newFilterCutoffMod +  // Aftertouch mod/modulation wheel mod
                           filterADSRMod; // ADSR mod
    filter.setCutoffFrequencyHz (jmax(0.001f, computedCutoff));
    //std::cout << " vel:" << filterCutoffVelMod << " mod:" << newFilterCutoffMod << " adsr:" << filterADSRMod << std::endl;
    //std::cout << filterCutoff << " to " << jmax(0.001f, computedCutoff) << std::endl;
    filter.setResonance (filterRessonance);
    
    // Amp and pan
    pan = sound->pan;
    auto& gain = processorChain.get<masterGainIndex>();
    float newGainMod;
    if (sound->mod2GainAmt >= 0){  // Set a maximum gain modulation combining mod wheel and aftertouch
        newGainMod = (float)jmin((double)(gainMod + sound->mod2GainAmt * (double)currentModWheelValue/127.0), (double)sound->mod2GainAmt);  // Add mod wheel modulation here
    } else {
        newGainMod = (float)jmax((double)(gainMod + sound->mod2GainAmt * (double)currentModWheelValue/127.0), (double)sound->mod2GainAmt);  // Add mod wheel modulation here
    }
    gain.setGainDecibels(sound->gain + newGainMod);
}

void SourceSamplerVoice::stopNote (float /*velocity*/, bool allowTailOff)
{
    if (allowTailOff) {
        // This is the case when receiving a note off event
        if (auto* sound = getCurrentlyPlayingSourceSamplerSound()){
            if (sound->getParameterInt("launchMode") == LAUNCH_MODE_TRIGGER){
                // We only trigger ADSRs release phase if we're in gate or loop launch modes, otherwise continue playing normally
            } else {
                adsr.noteOff();
                adsrFilter.noteOff();
            }
        }
    } else {
        // This is the case when we reached the end of the sound (or the end of the release stage) or for some other reason we want to cut abruptly
        clearCurrentNote();
    }
}

void SourceSamplerVoice::pitchWheelMoved (int newValue) {
    // 8192 = middle value
    // 16383 = max
    // 0 = min
    if (auto* sound = getCurrentlyPlayingSourceSamplerSound()){
        if (newValue >= 8192){
            pitchBendModSemitones = (double)(((float)newValue - 8192.0f)/8192.0f) * sound->pitchBendRangeUp;
        } else {
            pitchBendModSemitones = (double)((8192.0f - (float)newValue)/8192.0f) * sound->pitchBendRangeDown * -1;
        }
    }
}

void SourceSamplerVoice::aftertouchChanged(int newAftertouchValue)
{
    if (auto* sound = getCurrentlyPlayingSourceSamplerSound())
    {
        pitchModSemitones = sound->mod2PitchAmt * (double)newAftertouchValue/127.0;
        filterCutoffMod = (newAftertouchValue/127.0) * filterCutoff * sound->mod2CutoffAmt;
        gainMod = sound->mod2GainAmt * (float)newAftertouchValue/127.0;
        playheadSamplePositionMod = (double)newAftertouchValue/127.0;
    }
}


void SourceSamplerVoice::channelPressureChanged  (int newChannelPressureValue)
{
    if (auto* sound = getCurrentlyPlayingSourceSamplerSound())
    {
        pitchModSemitones = sound->mod2PitchAmt * (double)newChannelPressureValue/127.0;
        filterCutoffMod = (newChannelPressureValue/127.0) * filterCutoff * sound->mod2CutoffAmt;
        gainMod = sound->mod2GainAmt * (float)newChannelPressureValue/127.0;
        playheadSamplePositionMod = (double)newChannelPressureValue/127.0;
    }
}

void SourceSamplerVoice::controllerMoved (int controllerNumber, int newValue)
{
    // Set new modulation wheel value
    if (controllerNumber == 1){
        setModWheelValue(newValue);
    }
}

float interpolateSample (float samplePosition, const float* const signal)
{
    auto pos = (int) samplePosition;
    auto alpha = (float) (samplePosition - pos);
    auto invAlpha = 1.0f - alpha;
    return (signal[pos] * invAlpha + signal[pos + 1] * alpha);
}

//==============================================================================
void SourceSamplerVoice::renderNextBlock (AudioBuffer<float>& outputBuffer, int startSample, int numSamples)
{
    if (auto* sound = getCurrentlyPlayingSourceSamplerSound())
    {
        // Do some preparation (not all parameters will be used depending on the launch mode)
        int originalNumSamples = numSamples; // user later for filter processing
        double previousPitchRatio = pitchRatio;
        float previousPitchModSemitones = (float)jmin((double)pitchModSemitones + sound->mod2PitchAmt * (double)currentModWheelValue/127.0, (double)sound->mod2PitchAmt);  // Add mod wheel position
        float previousPitchBendModSemitones = pitchBendModSemitones;
        float previousPan = pan;
        bool noteStoppedHard = false;
        updateParametersFromSourceSamplerSound(sound);
        
        // Sampler reading and rendering
        auto& data = *sound->data;
        const float* const inL = data.getReadPointer (0);
        const float* const inR = data.getNumChannels() > 1 ? data.getReadPointer (1) : nullptr;
        
        tmpVoiceBuffer.clear();
        float* outL = tmpVoiceBuffer.getWritePointer (0, 0);
        float* outR = tmpVoiceBuffer.getNumChannels() > 1 ? tmpVoiceBuffer.getWritePointer (1, 0) : nullptr;
        
        
        while (--numSamples >= 0)
        {
            // Calculate L and R samples using basic interpolation
            float l = interpolateSample(playheadSamplePosition, inL);
            float r = (inR != nullptr) ? interpolateSample(playheadSamplePosition, inR) : l;
            
            if (sound->getParameterInt("launchMode") != LAUNCH_MODE_FREEZE){
                // Outside freeze mode, add samples from the source sound to the buffer and check for looping and other sorts of modulations
                
                // Check, in case we're looping, if we are in a crossfade zone and should do crossfade
                if ((sound->getParameterInt("launchMode") == LAUNCH_MODE_LOOP) && sound->loopXFadeNSamples > 0){
                    // NOTE: don't crossfade in LAUNCH_MODE_LOOP_FW_BW mode because it loops from the the same sample (no need to crossfade)
                    if (playheadDirectionIsForward){
                        // PLayhead going forward  (normal playing mode): do loop when reahing fixedLoopEndPositionSample
                        float samplesToLoopEndPositionSample = (float)fixedLoopEndPositionSample - playheadSamplePosition;
                        if ((samplesToLoopEndPositionSample > 0) && (samplesToLoopEndPositionSample < sound->loopXFadeNSamples)){
                            if (ENABLE_DEBUG_BUFFER == 1){
                                startRecordingToDebugBuffer((int)sound->loopXFadeNSamples * 2);
                            }
                            
                            // We are approaching loopEndPositionSample and are closer than sound->loopXFadeNSamples
                            float lcrossfadeSample = 0.0;
                            float rcrossfadeSample = 0.0;
                            float crossfadeGain = 0.0;
                            float crossfadePos = (float)fixedLoopStartPositionSample - samplesToLoopEndPositionSample;
                            if (crossfadePos > 0){
                                lcrossfadeSample = interpolateSample(crossfadePos, inL);
                                rcrossfadeSample = (inR != nullptr) ? interpolateSample(crossfadePos, inR) : lcrossfadeSample;
                                crossfadeGain = (float)samplesToLoopEndPositionSample/sound->loopXFadeNSamples;
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
                        if ((samplesToLoopStartPositionSample > 0) && (samplesToLoopStartPositionSample < sound->loopXFadeNSamples)){
                            // We are approaching loopStartPositionSample (going backwards) and are closer than sound->loopXFadeNSamples
                            float lcrossfadeSample = 0.0;
                            float rcrossfadeSample = 0.0;
                            float crossfadeGain = 0.0;
                            float crossfadePos = (float)fixedLoopEndPositionSample + samplesToLoopStartPositionSample;
                            if (crossfadePos < sound->length){
                                lcrossfadeSample = interpolateSample(crossfadePos, inL);
                                rcrossfadeSample = (inR != nullptr) ? interpolateSample(crossfadePos, inR) : lcrossfadeSample;
                                crossfadeGain = (float)samplesToLoopStartPositionSample/sound->loopXFadeNSamples;
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
            float rGainPan = jmin (1 - pan, 1.0f);
            float lGainPan = jmin (1 + pan, 1.0f);
            float previousRGainPan = jmin (1 - previousPan, 1.0f);
            float previousLGainPan = jmin (1 + previousPan, 1.0f);
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

            if (sound->getParameterInt("launchMode") == LAUNCH_MODE_FREEZE){
                // If in freeze mode, move from the current playhead position to the target playhead position in the length of the block
                double distanceTotargetPlayheadSamplePosition = targetPlayheadSamplePosition - playheadSamplePosition;
                double distanceTotargetPlayheadSamplePositionNormalized = std::abs(distanceTotargetPlayheadSamplePosition / sound->getLengthInSamples()); // normalized between 0 and 1
                double maxSpeed = jmax(std::pow(distanceTotargetPlayheadSamplePositionNormalized, 2) * sound->freezePlayheadSpeed, 1.0);
                double actualSpeed = jmin(maxSpeed, std::abs(distanceTotargetPlayheadSamplePosition));
                if (distanceTotargetPlayheadSamplePosition >= 0){
                    playheadSamplePosition += actualSpeed;
                } else {
                    playheadSamplePosition -= actualSpeed;
                }
                playheadSamplePosition = jlimit(0.0, (double)(sound->getLengthInSamples() - 1), playheadSamplePosition);  // Just to be sure...
 
            } else {
                // If not in freeze mode, advance source sample position for next iteration according to pitch ratio and other modulations...
                double interpolatedPitchRatio = (previousPitchRatio * ((double)numSamples/originalNumSamples) + pitchRatio * (1.0f - (double)numSamples/originalNumSamples));
                float interpolatedPitchModSemitones = (previousPitchModSemitones * ((float)numSamples/originalNumSamples) + pitchModSemitones * (1.0f - (float)numSamples/originalNumSamples));
                float interpolatedPitchBendModSemitones = (previousPitchBendModSemitones * ((float)numSamples/originalNumSamples) + pitchBendModSemitones * (1.0f - (float)numSamples/originalNumSamples));
                if (playheadDirectionIsForward){
                    playheadSamplePosition += interpolatedPitchRatio * std::pow(2, interpolatedPitchModSemitones/12) * std::pow(2, interpolatedPitchBendModSemitones/12);
                } else {
                    playheadSamplePosition -= interpolatedPitchRatio * std::pow(2, interpolatedPitchModSemitones/12) * std::pow(2, interpolatedPitchBendModSemitones/12);
                }
                
                // ... also check if we're reaching the end of the sound or looping region to do looping
                if ((sound->getParameterInt("launchMode") == LAUNCH_MODE_LOOP) || (sound->getParameterInt("launchMode") == LAUNCH_MODE_LOOP_FW_BW)){
                    // If looping is enabled, check whether we should loop
                    if (playheadDirectionIsForward){
                        if (playheadSamplePosition > fixedLoopEndPositionSample){
                            if (sound->getParameterInt("launchMode") == LAUNCH_MODE_LOOP_FW_BW) {
                                // Forward<>Backward loop mode (ping pong): stay on loop end but change direction
                                playheadDirectionIsForward = !playheadDirectionIsForward;
                            } else {
                                // Foward loop mode: jump from loop end to loop start
                                playheadSamplePosition = fixedLoopStartPositionSample;
                            }
                        }
                    } else {
                        if (playheadSamplePosition < fixedLoopStartPositionSample){
                            if (sound->getParameterInt("launchMode") == LAUNCH_MODE_LOOP_FW_BW) {
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
    }
}

void SourceSamplerVoice::prepare (const juce::dsp::ProcessSpec& spec)
{
    tmpVoiceBuffer = AudioBuffer<float>(spec.numChannels, spec.maximumBlockSize);
    processorChain.prepare (spec);
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
        debugBuffer = AudioBuffer<float>(1, bufferSize);
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

void SourceSamplerVoice::endRecordingToDebugBuffer(String outFilename){
    if (isRecordingToDebugBuffer){
        isRecordingToDebugBuffer = false;
        debugBufferCurrentPosition = 0;
        WavAudioFormat format;
        std::unique_ptr<AudioFormatWriter> writer;
        File file = File::getSpecialLocation(File::userDesktopDirectory).getChildFile(outFilename).withFileExtension("wav");
        writer.reset (format.createWriterFor (new FileOutputStream (file),
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

