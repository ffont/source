/*
  ==============================================================================

    SourceSamplerVoice.cpp
    Created: 3 Sep 2020 2:13:36pm
    Author:  Frederic Font Corbera

  ==============================================================================
*/

#include "SourceSampler.h"


// --------- implementation of SourceSamplerSound


SourceSamplerSound::SourceSamplerSound (int _idx,
                                        const String& soundName,
                                        AudioFormatReader& source,
                                        double maxSampleLengthSeconds,
                                        double _pluginSampleRate,
                                        int _pluginBlockSize)
    : idx (_idx),
      name (soundName),
      sourceSampleRate (source.sampleRate),
      pluginSampleRate (_pluginSampleRate),
      pluginBlockSize (_pluginBlockSize)
      
{
    if (sourceSampleRate > 0 && source.lengthInSamples > 0)
    {
        length = jmin ((int) source.lengthInSamples,
                       (int) (maxSampleLengthSeconds * sourceSampleRate));

        data.reset (new AudioBuffer<float> (jmin (2, (int) source.numChannels), length + 4));

        source.read (data.get(), 0, length + 4, 0, true, true);

    }
}

SourceSamplerSound::~SourceSamplerSound()
{
}

bool SourceSamplerSound::appliesToNote (int midiNoteNumber)
{
    return midiNotes[midiNoteNumber];
}

bool SourceSamplerSound::appliesToChannel (int /*midiChannel*/)
{
    return true;
}

void SourceSamplerSound::setParameterByNameFloat(const String& name, float value){
    // --> Start auto-generated code A
    if (name == "filterCutoff") { filterCutoff = jlimit(10.0f, 20000.0f, value); }
    else if (name == "filterRessonance") { filterRessonance = jlimit(0.0f, 1.0f, value); }
    else if (name == "maxPitchRatioMod") { maxPitchRatioMod = jlimit(0.0f, 2.0f, value); }
    else if (name == "maxFilterCutoffMod") { maxFilterCutoffMod = jlimit(0.0f, 100.0f, value); }
    else if (name == "filterKeyboardTracking") { filterKeyboardTracking = jlimit(-1.0f, 1.0f, value); }
    else if (name == "gain") { gain = jlimit(-80.0f, 12.0f, value); }
    else if (name == "ampADSR.attack") { ampADSR.attack = value; }
    else if (name == "ampADSR.decay") { ampADSR.decay = value; }
    else if (name == "ampADSR.sustain") { ampADSR.sustain = value; }
    else if (name == "ampADSR.release") { ampADSR.release = value; }
    else if (name == "filterADSR.attack") { filterADSR.attack = value; }
    else if (name == "filterADSR.decay") { filterADSR.decay = value; }
    else if (name == "filterADSR.sustain") { filterADSR.sustain = value; }
    else if (name == "filterADSR.release") { filterADSR.release = value; }
    else if (name == "maxFilterADSRMod") { maxFilterADSRMod = jlimit(0.0f, 10.0f, value); }
    else if (name == "basePitch") { basePitch = jlimit(-36.0f, 36.0f, value); }
    else if (name == "startPosition") { startPosition = jlimit(0.0f, 1.0f, value); }
    else if (name == "endPosition") { endPosition = jlimit(0.0f, 1.0f, value); }
    else if (name == "loopStartPosition") { loopStartPosition = jlimit(0.0f, 1.0f, value); }
    else if (name == "loopEndPosition") { loopEndPosition = jlimit(0.0f, 1.0f, value); }
    else if (name == "maxGainVelMod") { maxGainVelMod = jlimit(0.0f, 1.0f, value); }
    else if (name == "vel2CutoffAmt") { vel2CutoffAmt = jlimit(0.0f, 10.0f, value); }
    else if (name == "at2GainAmt") { at2GainAmt = jlimit(0.0f, 1.0f, value); }
    else if (name == "pan") { pan = jlimit(-1.0f, 1.0f, value); }
    else if (name == "pitchBendRangeUp") { pitchBendRangeUp = jlimit(0.0f, 36.0f, value); }
    else if (name == "pitchBendRangeDown") { pitchBendRangeDown = jlimit(0.0f, 36.0f, value); }
    // --> End auto-generated code A
    
    // Do some checking of start/end loop start/end positions to make sure we don't do anything wrong
    if (endPosition < startPosition){
        endPosition = startPosition;
    }
    if (loopStartPosition < startPosition){
        loopStartPosition = startPosition;
    }
    if (loopEndPosition > endPosition){
        loopEndPosition = endPosition;
    }
    if (loopStartPosition > loopEndPosition){
        loopStartPosition = loopEndPosition;
    }
}

void SourceSamplerSound::setParameterByNameInt(const String& name, int value){
    // --> Start auto-generated code C
    if (name == "midiRootNote") { midiRootNote = jlimit(0, 127, value); }
    else if (name == "loopXFadeNSamples") { loopXFadeNSamples = jlimit(10, 100000, value); }
    else if (name == "launchMode") { launchMode = jlimit(0, 3, value); }
    else if (name == "reverse") { reverse = jlimit(0, 1, value); }
    // --> End auto-generated code C
}

ValueTree SourceSamplerSound::getState(){
    ValueTree state = ValueTree(STATE_SAMPLER_SOUND);
    state.setProperty(STATE_SAMPLER_SOUND_MIDI_NOTES, midiNotes.toString(16), nullptr);
    state.setProperty(STATE_SOUND_INFO_IDX, idx, nullptr);  // idx is set to the state sot hat the UI gets it, but never loaded because it is generated dynamically
    
    // --> Start auto-generated code B
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "filterCutoff", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, filterCutoff, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "filterRessonance", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, filterRessonance, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "maxPitchRatioMod", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, maxPitchRatioMod, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "maxFilterCutoffMod", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, maxFilterCutoffMod, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "filterKeyboardTracking", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, filterKeyboardTracking, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "gain", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, gain, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "ampADSR.attack", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, ampADSR.attack, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "ampADSR.decay", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, ampADSR.decay, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "ampADSR.sustain", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, ampADSR.sustain, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "ampADSR.release", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, ampADSR.release, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "filterADSR.attack", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, filterADSR.attack, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "filterADSR.decay", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, filterADSR.decay, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "filterADSR.sustain", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, filterADSR.sustain, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "filterADSR.release", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, filterADSR.release, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "maxFilterADSRMod", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, maxFilterADSRMod, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "int", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "midiRootNote", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, midiRootNote, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "basePitch", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, basePitch, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "startPosition", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, startPosition, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "endPosition", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, endPosition, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "loopStartPosition", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, loopStartPosition, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "loopEndPosition", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, loopEndPosition, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "int", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "loopXFadeNSamples", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, loopXFadeNSamples, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "int", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "launchMode", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, launchMode, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "int", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "reverse", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, reverse, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "maxGainVelMod", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, maxGainVelMod, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "vel2CutoffAmt", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, vel2CutoffAmt, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "at2GainAmt", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, at2GainAmt, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "pan", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, pan, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "pitchBendRangeUp", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, pitchBendRangeUp, nullptr),
                      nullptr);
    state.appendChild(ValueTree(STATE_SAMPLER_SOUND_PARAMETER)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE, "float", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "pitchBendRangeDown", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, pitchBendRangeDown, nullptr),
                      nullptr);
    // --> End auto-generated code B
    
    return state;
}

void SourceSamplerSound::loadState(ValueTree soundState){
    
    if (soundState.hasProperty(STATE_SAMPLER_SOUND_MIDI_NOTES)){
        midiNotes.parseString(soundState.getProperty(STATE_SAMPLER_SOUND_MIDI_NOTES).toString(), 16);
    }
    
    // Iterate over parameters and load them
    if (soundState.isValid()){
        for (int i=0; i<soundState.getNumChildren(); i++){
            ValueTree parameter = soundState.getChild(i);
            String type = parameter.getProperty(STATE_SAMPLER_SOUND_PARAMETER_TYPE).toString();
            String name = parameter.getProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME).toString();
            if (type == "float"){
                float value = (float)parameter.getProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE);
                setParameterByNameFloat(name, value);
            } else if (type == "int"){
                int value = (int)parameter.getProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE);
                setParameterByNameInt(name, value);
            }
        }
    }
}

int SourceSamplerSound::getLengthInSamples(){
    return length;
}

float SourceSamplerSound::getLengthInSeconds(){
    return (float)getLengthInSamples()/sourceSampleRate;
}

int SourceSamplerSound::getIdx(){
    return idx;
}

// --------- implementation of SourceSamplerVoice


SourceSamplerVoice::SourceSamplerVoice() {}

SourceSamplerVoice::~SourceSamplerVoice() {}

SourceSamplerSound* SourceSamplerVoice::getCurrentlyPlayingSourceSamplerSound() const noexcept {
    return static_cast<SourceSamplerSound*> (getCurrentlyPlayingSound().get());
}

bool SourceSamplerVoice::canPlaySound (SynthesiserSound* sound)
{
    return dynamic_cast<const SourceSamplerSound*> (sound) != nullptr;
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

void SourceSamplerVoice::updateParametersFromSourceSamplerSound(SourceSamplerSound* sound)
{
    // This is called at each processing block of 64 samples
    
    // Pitch
    double currentNoteFrequency = std::pow (2.0, (sound->basePitch + getCurrentlyPlayingNote() - sound->midiRootNote + pitchBendModSemitones) / 12.0);
    pitchRatio = currentNoteFrequency * sound->sourceSampleRate / getSampleRate();
    
    // Looping settings
    int soundLengthInSamples = sound->getLengthInSamples();
    startPositionSample = (int)(sound->startPosition * soundLengthInSamples);
    endPositionSample = (int)(sound->endPosition * soundLengthInSamples);
    int soundLoopStartPosition = (int)(sound->loopStartPosition * soundLengthInSamples);
    int soundLoopEndPosition = (int)(sound->loopEndPosition * soundLengthInSamples);
    if ((soundLoopStartPosition != loopStartPositionSample) || (soundLoopEndPosition != loopEndPositionSample)){
        // Either loop start or end has changed in the sound object
        auto& data = *sound->data;
        const float* const signal = data.getReadPointer (0);  // use first audio channel to detect 0 crossing
        if (soundLoopStartPosition != loopStartPositionSample){
            // If the loop start position has changed, process it to move it to the next positive zero crossing
            fixedLoopStartPositionSample = findNearestPositiveZeroCrossing(soundLoopStartPosition, signal, 2000);
            DBG("Fixed start sample position by " << fixedLoopStartPositionSample - soundLoopStartPosition);
        }
        if (soundLoopEndPosition != loopEndPositionSample){
            // If the loop end position has changed, process it to move it to the next positive zero crossing
            fixedLoopEndPositionSample = findNearestPositiveZeroCrossing(soundLoopEndPosition, signal, -2000);
            DBG("Fixed end sample position by " << fixedLoopEndPositionSample - soundLoopEndPosition);
        }
    }
    loopStartPositionSample = soundLoopStartPosition;
    loopEndPositionSample = soundLoopEndPosition;
    
    // ADSRs
    adsr.setParameters (sound->ampADSR);
    adsrFilter.setParameters (sound->filterADSR);
    
    // Filter
    filterCutoff = sound->filterCutoff; // * std::pow(2, getCurrentlyPlayingNote() - sound->midiRootNote) * sound->filterKeyboardTracking;  // Add kb tracking
    filterRessonance = sound->filterRessonance;
    auto& filter = processorChain.get<filterIndex>();
    float computedCutoff = filterCutoff  + // Base cutoff
                           currentNoteFrequency * std::pow(2, (getCurrentlyPlayingNote() - 64)/12) * sound->filterKeyboardTracking + // Add kb tracking
                           filterCutoffVelMod + // Velocity mod to cutoff
                           filterCutoffMod +  // Aftertouch mod/modulation wheel mod
                           sound->maxFilterADSRMod * filterCutoff * adsrFilter.getNextSample(); // ADSR mod
    filter.setCutoffFrequencyHz (jmax(0.001f, computedCutoff));
    filter.setResonance (filterRessonance);
    
    // Amp and pan
    pan = sound->pan;
    masterGain = Decibels::decibelsToGain(sound->gain);  // From dB to linear (note gain in SamplerSound is really in dB)
    masterGain = masterGain + gainMod; // Add modulation gain (if any)
    auto& gain = processorChain.get<masterGainIndex>();
    gain.setGainLinear (1.0); // This gain we don't really use it
}

void SourceSamplerVoice::startNote (int midiNoteNumber, float velocity, SynthesiserSound* s, int /*currentPitchWheelPosition*/)
{
    // This is called when note on is received
    if (auto* sound = dynamic_cast<SourceSamplerSound*> (s))
    {
        // Reset processor chain internal state
        processorChain.reset();
        
        // Reset some parameters
        adsr.reset();
        adsrFilter.reset();
        pitchRatioMod = 0.0;
        pitchBendModSemitones = 0.0;
        filterCutoffMod = 0.0;
        
        // Load and configure parameters from SourceSamplerSound
        adsr.setSampleRate (sound->pluginSampleRate);
        // TODO: what should really be the sample rate below?
        adsrFilter.setSampleRate (sound->sourceSampleRate/sound->pluginBlockSize); // Lower sample rate because we only update filter cutoff once per processing block...
        
        // Compute velocity modulations (only relevant at start of note)
        filterCutoffVelMod = sound->filterCutoff * sound->vel2CutoffAmt * velocity;
        float velocityGain = (sound->maxGainVelMod * velocity) + (1 - sound->maxGainVelMod);
        lgain = velocityGain;
        rgain = velocityGain;
        
        // Update the rest of parameters (that will be udpated at each block)
        updateParametersFromSourceSamplerSound(sound);
        
        // Set initial playhead position
        if (sound->reverse == 0){
            sourceSamplePosition = startPositionSample;
            playheadDirectionIsForward = true;
        } else {
            sourceSamplePosition = endPositionSample;
            playheadDirectionIsForward = false;
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

void SourceSamplerVoice::stopNote (float /*velocity*/, bool allowTailOff)
{
    if (allowTailOff) {
        // This is the case when receiving a note off event
        if (auto* sound = getCurrentlyPlayingSourceSamplerSound()){
            if (sound->launchMode == LAUNCH_MODE_TRIGGER){
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
        // Aftertouch modifies the playback speed up to an octave
        pitchRatioMod = sound->maxPitchRatioMod * pitchRatio * (double)newAftertouchValue/127.0;
        
        // Aftertouch also modifies filter cutoff
        filterCutoffMod = sound->maxFilterCutoffMod * filterCutoff * (double)newAftertouchValue/127.0;
        processorChain.get<filterIndex>().setCutoffFrequencyHz (filterCutoff + filterCutoffMod);
        
        //Aftertouch modifies gain as well
        gainMod = sound->at2GainAmt * (float)newAftertouchValue/127.0; // This assumes at2AmpMultiplierAmt goes from 1.0 to X, but minimum is always 1.0
    }
}


void SourceSamplerVoice::channelPressureChanged  (int newChannelPressureValue)
{
    if (auto* sound = getCurrentlyPlayingSourceSamplerSound())
    {
        // Channel aftertouch modifies the playback speed up to an octave
        pitchRatioMod = sound->maxPitchRatioMod * pitchRatio * (double)newChannelPressureValue/127.0;
        
        // Channel aftertouch also modifies filter cutoff
        filterCutoffMod = sound->maxFilterCutoffMod * filterCutoff * (double)newChannelPressureValue/127.0;
        processorChain.get<filterIndex>().setCutoffFrequencyHz (filterCutoff + filterCutoffMod);
        
        // Channel aftertouch modifies gain as well
        gainMod = sound->at2GainAmt * (float)newChannelPressureValue/127.0; // This assumes at2AmpMultiplierAmt goes from 1.0 to X, but minimum is always 1.0
    }
}

void SourceSamplerVoice::controllerMoved (int controllerNumber, int newValue) {
    
    if (controllerNumber == 1){
        if (auto* sound = getCurrentlyPlayingSourceSamplerSound())
        {
            // Modulation wheel modifies the playback speed up to an octave
            pitchRatioMod = sound->maxPitchRatioMod * pitchRatio * (double)newValue/127.0;
            
            // Modulation wheel also modifies filter cutoff
            filterCutoffMod = sound->maxFilterCutoffMod * filterCutoff * (double)newValue/127.0;
            processorChain.get<filterIndex>().setCutoffFrequencyHz (filterCutoff + filterCutoffMod);
            
            // Modulation wheel modifies gain as well
            gainMod = sound->at2GainAmt * (float)newValue/127.0; // This assumes at2AmpMultiplierAmt goes from 1.0 to X, but minimum is always 1.0
        }
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
        // Do some preparation
        int originalNumSamples = numSamples; // user later for filter processing
        float previousMasterGain = masterGain;
        float previousPitchRatioMod = pitchRatioMod;
        float previousPan = pan;
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
            float l = interpolateSample(sourceSamplePosition, inL);
            float r = (inR != nullptr) ? interpolateSample(sourceSamplePosition, inR) : l;
            // Check, in case we're looping, if we are in a crossfade zone and should do crossfade
            if ((sound->launchMode == LAUNCH_MODE_LOOP) && sound->loopXFadeNSamples > 0){
                // NOTE: don't crossfade in LAUNCH_MODE_LOOP_FW_BW mode because it loops from the the same sample (no need to crossfade)
                if (playheadDirectionIsForward){
                    // PLayhead going forward  (normal playing mode): do loop when reahing fixedLoopEndPositionSample
                    float samplesToLoopEndPositionSample = (float)fixedLoopEndPositionSample - sourceSamplePosition;
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
                    int samplesToLoopStartPositionSample = sourceSamplePosition - (float)fixedLoopStartPositionSample;
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
            
            if (ENABLE_DEBUG_BUFFER == 1){
                writeToDebugBuffer(l);
            }
            
            // Draw envelope sample and add it to L and R samples, also add panning and velocity gain
            float interpolatedMasterGain = (previousMasterGain * ((float)numSamples/originalNumSamples) + masterGain * (1.0f - (float)numSamples/originalNumSamples));
            float rGainPan = jmin (1 - pan, 1.0f);
            float lGainPan = jmin (1 + pan, 1.0f);
            float previousRGainPan = jmin (1 - previousPan, 1.0f);
            float previousLGainPan = jmin (1 + previousPan, 1.0f);
            float interpolatedRGainPan = (previousRGainPan * ((float)numSamples/originalNumSamples) + rGainPan * (1.0f - (float)numSamples/originalNumSamples));
            float interpolatedLGainPan = (previousLGainPan * ((float)numSamples/originalNumSamples) + lGainPan * (1.0f - (float)numSamples/originalNumSamples));
            auto envelopeValue = adsr.getNextSample();
            
            l *= lgain * interpolatedLGainPan * envelopeValue * interpolatedMasterGain;
            r *= rgain * interpolatedRGainPan * envelopeValue * interpolatedMasterGain;

            // Update output buffer with L and R samples
            if (outR != nullptr) {
                *outL++ += l;
                *outR++ += r;
            } else {
                *outL++ += (l + r) * 0.5f;
            }

            // Advance source sample position for next iteration
            float interpolatedPitchRatioMod = (previousPitchRatioMod * ((float)numSamples/originalNumSamples) + pitchRatioMod * (1.0f - (float)numSamples/originalNumSamples));;
            if (playheadDirectionIsForward){
                sourceSamplePosition += pitchRatio + interpolatedPitchRatioMod;
            } else {
                sourceSamplePosition -= pitchRatio + interpolatedPitchRatioMod;
            }
            
            // Check if we're reaching the end of the sound
            bool noteStoppedHard = false;
            if ((sound->launchMode == LAUNCH_MODE_LOOP) || (sound->launchMode == LAUNCH_MODE_LOOP_FW_BW)){
                // If looping is enabled, check whether we should loop
                if (playheadDirectionIsForward){
                    if (sourceSamplePosition > fixedLoopEndPositionSample){
                        if (sound->launchMode == LAUNCH_MODE_LOOP_FW_BW) {
                            // Forward<>Backward loop mode (ping pong): stay on loop end but change direction
                            playheadDirectionIsForward = !playheadDirectionIsForward;
                        } else {
                            // Foward loop mode: jump from loop end to loop start
                            sourceSamplePosition = fixedLoopStartPositionSample;
                        }
                    }
                } else {
                    if (sourceSamplePosition < fixedLoopStartPositionSample){
                        if (sound->launchMode == LAUNCH_MODE_LOOP_FW_BW) {
                            // Forward<>Backward loop mode (ping pong): stay on loop end but change direction
                            playheadDirectionIsForward = !playheadDirectionIsForward;
                        } else {
                            // Forward loop mode (in reverse): jump from loop start to loop end
                            sourceSamplePosition = fixedLoopEndPositionSample;
                        }
                    }
                }
            } else {
                // If not looping, check whether we've reached the end of the file
                bool endReached = false;
                if (playheadDirectionIsForward){
                    if (sourceSamplePosition > endPositionSample){
                        endReached = true;
                    }
                } else {
                    if (sourceSamplePosition < startPositionSample){
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
        return (float)sourceSamplePosition/(float)sound->getLengthInSamples();
    }
    return -1.0;
}


void SourceSamplerVoice::startRecordingToDebugBuffer(int bufferSize){
    if (!isRecordingToDebugBuffer && !debugBufferFinishedRecording){
        DBG("Started writing to buffer...");
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
            DBG("Writing buffer to file...");
        }
    }
}


