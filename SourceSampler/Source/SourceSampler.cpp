/*
  ==============================================================================

    SourceSamplerVoice.cpp
    Created: 3 Sep 2020 2:13:36pm
    Author:  Frederic Font Corbera

  ==============================================================================
*/

#include "SourceSampler.h"


// --------- implementation of SourceSamplerSound


SourceSamplerSound::SourceSamplerSound (const String& soundName,
                                        AudioFormatReader& source,
                                        const BigInteger& notes,
                                        int midiNoteForNormalPitch,
                                        double maxSampleLengthSeconds,
                                        double _pluginSampleRate,
                                        int _pluginBlockSize)
    : name (soundName),
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
    if (name == "filterCutoff") { filterCutoff = jlimit(0.0f, 20000.0f, value); }
    else if (name == "filterRessonance") { filterRessonance = jlimit(0.0f, 1.0f, value); }
    else if (name == "maxPitchRatioMod") { maxPitchRatioMod = jlimit(0.0f, 2.0f, value); }
    else if (name == "maxFilterCutoffMod") { maxFilterCutoffMod = jlimit(0.0f, 10.0f, value); }
    else if (name == "gain") { gain = jlimit(0.0f, 1.0f, value); }
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
    // --> End auto-generated code A
}

void SourceSamplerSound::setParameterByNameInt(const String& name, int value){
    // --> Start auto-generated code C
    if (name == "loopMode") { loopMode = jlimit(0, 1, value); }
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
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_NAME, "loopMode", nullptr)
                      .setProperty(STATE_SAMPLER_SOUND_PARAMETER_VALUE, loopMode, nullptr),
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
        }
    }
}

int SourceSamplerSound::getLengthInSamples(){
    return length;
}

float SourceSamplerSound::getLengthInSeconds(){
    return (float)getLengthInSamples()/sourceSampleRate;
}


// --------- implementation of SourceSamplerVoice


SourceSamplerVoice::SourceSamplerVoice() {
    
    auto& filter = processorChain.get<filterIndex>();
    filter.setCutoffFrequencyHz (filterCutoff);
    filter.setResonance (filterRessonance);
    
    auto& gain = processorChain.get<masterGainIndex>();
    gain.setGainLinear (masterGain);
    
}

SourceSamplerVoice::~SourceSamplerVoice() {}

bool SourceSamplerVoice::canPlaySound (SynthesiserSound* sound)
{
    return dynamic_cast<const SourceSamplerSound*> (sound) != nullptr;
}

void SourceSamplerVoice::updateParametersFromSourceSamplerSound(SourceSamplerSound* sound)
{
    // This is called at each processing block of 64 samples
    
    // Pitch
    pitchRatio = std::pow (2.0, (sound->basePitch + midiNoteCurrentlyPlayed - sound->midiRootNote) / 12.0) * sound->sourceSampleRate / getSampleRate();
    
    // Looping settings
    int soundLengthInSamples = sound->getLengthInSamples();
    startPositionSample = (int)(sound->startPosition * soundLengthInSamples);
    endPositionSample = (int)(sound->endPosition * soundLengthInSamples);
    loopStartPositionSample = (int)(sound->loopStartPosition * soundLengthInSamples);
    loopEndPositionSample = (int)(sound->loopEndPosition * soundLengthInSamples);
    doLoop = sound->loopMode == 1;
    //loopCroosfadeNSamples = 100;
    
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
    masterGain = sound->gain;
    auto& gain = processorChain.get<masterGainIndex>();
    gain.setGainLinear (masterGain);

}

void SourceSamplerVoice::startNote (int midiNoteNumber, float velocity, SynthesiserSound* s, int /*currentPitchWheelPosition*/)
{
    // This is called when note on is received
    
    if (auto* sound = dynamic_cast<SourceSamplerSound*> (s))
    {
        midiNoteCurrentlyPlayed = midiNoteNumber;
        
        // Reset some parameters
        adsr.reset();
        adsrFilter.reset();
        pitchRatioMod = 0.0;
        filterCutoffMod = 0.0;
        isInReleaseADSRStage = false;
        
        // Load and configure parameters from SourceSamplerSound
        adsr.setSampleRate (sound->pluginSampleRate);
        // TODO: what should really be the sample rate below?
        adsrFilter.setSampleRate (sound->sourceSampleRate/sound->pluginBlockSize); // Lower sample rate because we only update filter cutoff once per processing block...
        
        updateParametersFromSourceSamplerSound(sound);
        
        sourceSamplePosition = startPositionSample;
        lgain = velocity;
        rgain = velocity;

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
    if (allowTailOff)
    {
        adsr.noteOff();
        adsrFilter.noteOff();
        isInReleaseADSRStage = true;
    }
    else
    {
        clearCurrentNote();
        //adsr.reset();
        //adsrFilter.reset();
    }
}

void SourceSamplerVoice::pitchWheelMoved (int newValue) {
}

void SourceSamplerVoice::aftertouchChanged(int newAftertouchValue)
{
    if (auto* playingSound = static_cast<SourceSamplerSound*> (getCurrentlyPlayingSound().get()))
    {
        // Aftertouch modifies the playback speed up to an octave
        pitchRatioMod = playingSound->maxPitchRatioMod * pitchRatio * (double)newAftertouchValue/127.0;
        
        // Aftertouch also modified filter cutoff
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
        
        // Aftertouch also modified filter cutoff
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
            
            // Aftertouch also modified filter cutoff
            filterCutoffMod = playingSound->maxFilterCutoffMod * filterCutoff * (double)newValue/127.0;
            processorChain.get<filterIndex>().setCutoffFrequencyHz (filterCutoff + filterCutoffMod);
        }
    }
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

        while (--numSamples >= 0)
        {
            auto pos = (int) sourceSamplePosition;
            auto alpha = (float) (sourceSamplePosition - pos);
            auto invAlpha = 1.0f - alpha;

            // just using a very simple linear interpolation here..
            float l = (inL[pos] * invAlpha + inL[pos + 1] * alpha);
            float r = (inR != nullptr) ? (inR[pos] * invAlpha + inR[pos + 1] * alpha)
                                       : l;

            auto envelopeValue = adsr.getNextSample();

            l *= lgain * envelopeValue;
            r *= rgain * envelopeValue;

            if (outR != nullptr)
            {
                *outL++ += l;
                *outR++ += r;
            }
            else
            {
                *outL++ += (l + r) * 0.5f;
            }

            sourceSamplePosition += pitchRatio + pitchRatioMod;
            
            if (doLoop && !isInReleaseADSRStage){
                // If looping is enabled and we're not yet in release stage, check whether we should loop
                if (sourceSamplePosition > loopEndPositionSample){
                    sourceSamplePosition = loopStartPositionSample;
                }
                
            } else {
                // If not looping or looping but already in release stage, check whether we've reached the end of the file
                if (sourceSamplePosition > endPositionSample)
                {
                    stopNote (0.0f, false);
                    break;
                }
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
