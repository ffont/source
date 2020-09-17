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
                                        const BigInteger& notes,
                                        int midiNoteForNormalPitch,
                                        double maxSampleLengthSeconds,
                                        double _pluginSampleRate,
                                        int _pluginBlockSize)
    : idx (_idx),
      name (soundName),
      sourceSampleRate (source.sampleRate),
      midiNotes (notes),
      midiRootNote (midiNoteForNormalPitch),
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
    
}

void SourceSamplerSound::setParameterByNameInt(const String& name, int value){
    // --> Start auto-generated code C
    if (name == "loopXFadeNSamples") { loopXFadeNSamples = jlimit(10, 10000, value); }
    else if (name == "launchMode") { launchMode = jlimit(0, 2, value); }
    else if (name == "reverse") { reverse = jlimit(0, 1, value); }
    // --> End auto-generated code C
}

ValueTree SourceSamplerSound::getState(){
    ValueTree state = ValueTree(STATE_SAMPLER_SOUND);
    
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
    
    // Iterate over parameters and load them
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


SourceSamplerVoice::SourceSamplerVoice() {
}

SourceSamplerVoice::~SourceSamplerVoice() {}

bool SourceSamplerVoice::canPlaySound (SynthesiserSound* sound)
{
    return dynamic_cast<const SourceSamplerSound*> (sound) != nullptr;
}

int findNearestPositiveZeroCrossing (int position, const float* const signal, int maxSamplesSearch)
{
    if (maxSamplesSearch > 0){
        // Search forward
        for (int i=position; i<position + maxSamplesSearch; i++){
            if ((signal[i] < 0) && (signal[i+1] >= 0)){
                return i;
            }
        }
    } else {
        // Search backwards
        for (int i=position; i<position - maxSamplesSearch; i--){
            if ((signal[i-1] < 0) && (signal[i] >= 0)){
                return i;
            }
        }
    }
    
    return position;
}

void SourceSamplerVoice::updateParametersFromSourceSamplerSound(SourceSamplerSound* sound)
{
    // This is called at each processing block of 64 samples
    
    // Pitch
    pitchRatio = std::pow (2.0, (sound->basePitch + getCurrentlyPlayingNote() - sound->midiRootNote + pitchBendModSemitones) / 12.0) * sound->sourceSampleRate / getSampleRate();
    
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
            fixedLoopStartPositionSample = findNearestPositiveZeroCrossing(soundLoopStartPosition, signal, 10000);
        }
        if (soundLoopEndPosition != loopEndPositionSample){
            // If the loop end position has changed, process it to move it to the next positive zero crossing
            fixedLoopEndPositionSample = findNearestPositiveZeroCrossing(soundLoopEndPosition, signal, -10000);
        }
    }
    loopStartPositionSample = soundLoopStartPosition;
    loopEndPositionSample = soundLoopEndPosition;
    
    // ADSRs
    adsr.setParameters (sound->ampADSR);
    adsrFilter.setParameters (sound->filterADSR);
    
    // Filter
    filterCutoff = sound->filterCutoff;
    filterRessonance = sound->filterRessonance;
    auto& filter = processorChain.get<filterIndex>();
    filter.setCutoffFrequencyHz (filterCutoff + filterCutoffMod + sound->maxFilterADSRMod * filterCutoff * adsrFilter.getNextSample()); // Base cutoff + AT mod + ADSR mod
    filter.setResonance (filterRessonance);
    
    // Amp
    masterGain = Decibels::decibelsToGain(sound->gain);  // From dB to linear (note gain in SamplerSound is really in dB)
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
        
        updateParametersFromSourceSamplerSound(sound);
        
        if (sound->reverse == 0){
            sourceSamplePosition = startPositionSample;
        } else {
            sourceSamplePosition = endPositionSample;
        }
        
        float velocityGain = (sound->maxGainVelMod * velocity) + (1 - sound->maxGainVelMod);
        lgain = velocityGain;
        rgain = velocityGain;

        // Trigger ADSR
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
        if (auto* playingSound = static_cast<SourceSamplerSound*> (getCurrentlyPlayingSound().get())){
            if (playingSound->launchMode == LAUNCH_MODE_TRIGGER){
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
    if (auto* playingSound = static_cast<SourceSamplerSound*> (getCurrentlyPlayingSound().get()))
    {
        if (newValue >= 8192){
            pitchBendModSemitones = (double)(((float)newValue - 8192.0f)/8192.0f) * playingSound->pitchBendRangeUp;
        } else {
            pitchBendModSemitones = (double)((8192.0f - (float)newValue)/8192.0f) * playingSound->pitchBendRangeDown * -1;
        }
    }
}

void SourceSamplerVoice::aftertouchChanged(int newAftertouchValue)
{
    if (auto* playingSound = static_cast<SourceSamplerSound*> (getCurrentlyPlayingSound().get()))
    {
        // Aftertouch modifies the playback speed up to an octave
        pitchRatioMod = playingSound->maxPitchRatioMod * pitchRatio * (double)newAftertouchValue/127.0;
        
        // Aftertouch also modifies filter cutoff
        filterCutoffMod = playingSound->maxFilterCutoffMod * filterCutoff * (double)newAftertouchValue/127.0;
        processorChain.get<filterIndex>().setCutoffFrequencyHz (filterCutoff + filterCutoffMod);
    }
}


void SourceSamplerVoice::channelPressureChanged  (int newChannelPressureValue)
{
    if (auto* playingSound = static_cast<SourceSamplerSound*> (getCurrentlyPlayingSound().get()))
    {
        // Channel aftertouch modifies the playback speed up to an octave
        pitchRatioMod = playingSound->maxPitchRatioMod * pitchRatio * (double)newChannelPressureValue/127.0;
        
        // Aftertouch also modifies filter cutoff
        filterCutoffMod = playingSound->maxFilterCutoffMod * filterCutoff * (double)newChannelPressureValue/127.0;
        processorChain.get<filterIndex>().setCutoffFrequencyHz (filterCutoff + filterCutoffMod);
    }
}

void SourceSamplerVoice::controllerMoved (int controllerNumber, int newValue) {
    
    if (controllerNumber == 1){
        if (auto* playingSound = static_cast<SourceSamplerSound*> (getCurrentlyPlayingSound().get()))
        {
            // Channel aftertouch modifies the playback speed up to an octave
            pitchRatioMod = playingSound->maxPitchRatioMod * pitchRatio * (double)newValue/127.0;
            
            // Aftertouch also modifies filter cutoff
            filterCutoffMod = playingSound->maxFilterCutoffMod * filterCutoff * (double)newValue/127.0;
            processorChain.get<filterIndex>().setCutoffFrequencyHz (filterCutoff + filterCutoffMod);
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
    if (auto* playingSound = static_cast<SourceSamplerSound*> (getCurrentlyPlayingSound().get()))
    {
        // Do some preparation
        int originalNumSamples = numSamples; // user later for filter processing
        updateParametersFromSourceSamplerSound(playingSound);
        
        // Sampler reading and rendering
        auto& data = *playingSound->data;
        const float* const inL = data.getReadPointer (0);
        const float* const inR = data.getNumChannels() > 1 ? data.getReadPointer (1) : nullptr;

        float* outL = outputBuffer.getWritePointer (0, startSample);
        float* outR = outputBuffer.getNumChannels() > 1 ? outputBuffer.getWritePointer (1, startSample) : nullptr;
        
        // Compute panner gain
        float rgainPan = jmin (1 - playingSound->pan, 1.0f);
        float lgainPan = jmin (1 + playingSound->pan, 1.0f);

        while (--numSamples >= 0)
        {
            // Calculate L and R samples using basic interpolation
            float l = interpolateSample(sourceSamplePosition, inL);
            float r = (inR != nullptr) ? interpolateSample(sourceSamplePosition, inR)
                                       : l;
            // Check, in case we're looping, if we are in a crossfade zone and should do crossfade
            if (playingSound->launchMode == LAUNCH_MODE_LOOP && playingSound->loopXFadeNSamples > 0){
                if (playingSound->reverse == 0){
                    // Normal playing mode: do loop when reahing fixedLoopEndPositionSample
                    int samplesToLoopEndPositionSample = fixedLoopEndPositionSample - sourceSamplePosition;
                    if ((samplesToLoopEndPositionSample > 0) && (samplesToLoopEndPositionSample < playingSound->loopXFadeNSamples)){
                        // We are approaching loopEndPositionSample and are closer than playingSound->loopXFadeNSamples
                        float lcrossfadeSample = 0.0;
                        float rcrossfadeSample = 0.0;
                        float crossfadeGain = 0.0;
                        int crossfadePos = fixedLoopStartPositionSample - samplesToLoopEndPositionSample;
                        if (crossfadePos > 0){
                            lcrossfadeSample = interpolateSample(crossfadePos, inL);
                            rcrossfadeSample = (inR != nullptr) ? interpolateSample(crossfadePos, inR)
                                                                : lcrossfadeSample;
                            crossfadeGain = (float)samplesToLoopEndPositionSample/playingSound->loopXFadeNSamples;
                        } else {
                            // If position is negative, there is no data to do the crossfade
                        }
                        l = l * (crossfadeGain) + lcrossfadeSample * (1.0f - crossfadeGain);
                        r = r * (crossfadeGain) + rcrossfadeSample * (1.0f - crossfadeGain);
                    } else {
                        // Do nothing because we're not in crossfade zone
                    }
                } else {
                    // Reverse playing mode: do loop when reahing fixedLoopEndPositionSample
                    int samplesToLoopStartPositionSample = sourceSamplePosition - fixedLoopStartPositionSample;
                    if ((samplesToLoopStartPositionSample > 0) && (samplesToLoopStartPositionSample < playingSound->loopXFadeNSamples)){
                        // We are approaching loopStartPositionSample (going backwards) and are closer than playingSound->loopXFadeNSamples
                        float lcrossfadeSample = 0.0;
                        float rcrossfadeSample = 0.0;
                        float crossfadeGain = 0.0;
                        int crossfadePos = fixedLoopEndPositionSample + samplesToLoopStartPositionSample;
                        if (crossfadePos < playingSound->length){
                            lcrossfadeSample = interpolateSample(crossfadePos, inL);
                            rcrossfadeSample = (inR != nullptr) ? interpolateSample(crossfadePos, inR) : lcrossfadeSample;
                            crossfadeGain = (float)samplesToLoopStartPositionSample/playingSound->loopXFadeNSamples;
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
            
            // Draw envelope sample and add it to L and R samples, also add panning and velocity gain
            auto envelopeValue = adsr.getNextSample();
            l *= lgain * lgainPan * envelopeValue * masterGain;
            r *= rgain * rgainPan * envelopeValue * masterGain;

            // Update output buffer with L and R samples
            if (outR != nullptr) {
                *outL++ += l;
                *outR++ += r;
            } else {
                *outL++ += (l + r) * 0.5f;
            }

            // Advance source sample position for next iteration
            if (playingSound->reverse == 0){
                sourceSamplePosition += pitchRatio + pitchRatioMod;
            } else {
                sourceSamplePosition -= pitchRatio + pitchRatioMod;
            }
            
            // Check if we're reaching the end of the sound
            bool noteStoppedHard = false;
            if (playingSound->launchMode == LAUNCH_MODE_LOOP){
                // If looping is enabled, check whether we should loop
                if (playingSound->reverse == 0){
                    if (sourceSamplePosition > fixedLoopEndPositionSample){
                        sourceSamplePosition = fixedLoopStartPositionSample;
                    }
                } else {
                    if (sourceSamplePosition < fixedLoopStartPositionSample){
                        sourceSamplePosition = fixedLoopEndPositionSample;
                    }
                }
            } else {
                // If not looping, check whether we've reached the end of the file
                bool endReached = false;
                if (playingSound->reverse == 0){
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

        // Filter and master gain processing
        auto block = juce::dsp::AudioBlock<float> (outputBuffer);
        auto blockToUse = block.getSubBlock ((size_t) startSample, (size_t) originalNumSamples);
        auto contextToUse = juce::dsp::ProcessContextReplacing<float> (blockToUse);
        processorChain.process (contextToUse);
    }
}

void SourceSamplerVoice::prepare (const juce::dsp::ProcessSpec& spec)
{
    processorChain.prepare (spec);
}

float SourceSamplerVoice::getPlayingPositionPercentage()
{
    if (auto* playingSound = static_cast<SourceSamplerSound*> (getCurrentlyPlayingSound().get()))
    {
        return (float)sourceSamplePosition/(float)playingSound->getLengthInSamples();
    }
    return -1.0;
}
