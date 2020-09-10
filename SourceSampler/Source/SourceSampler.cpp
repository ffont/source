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
                                        double maxSampleLengthSeconds)
    : name (soundName),
      sourceSampleRate (source.sampleRate),
      midiNotes (notes),
      midiRootNote (midiNoteForNormalPitch)
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
    // --> End auto-generated code A
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
    filterCutoff = sound->filterCutoff;
    filterRessonance = sound->filterRessonance;
    auto& filter = processorChain.get<filterIndex>();
    filter.setCutoffFrequencyHz (filterCutoff + filterCutoffMod);
    filter.setResonance (filterRessonance);
    
    masterGain = sound->gain;
    auto& gain = processorChain.get<masterGainIndex>();
    gain.setGainLinear (masterGain);
    
    adsr.setParameters (sound->ampADSR);
}

void SourceSamplerVoice::startNote (int midiNoteNumber, float velocity, SynthesiserSound* s, int /*currentPitchWheelPosition*/)
{
    if (auto* sound = dynamic_cast<SourceSamplerSound*> (s))
    {
        // Reset some parameters
        pitchRatioMod = 0.0;
        filterCutoffMod = 0.0;
        
        // Load and configure parameters from SourceSamplerSound
        adsr.setSampleRate (sound->sourceSampleRate);
        updateParametersFromSourceSamplerSound(sound);
        
        // Initialize other variables
        pitchRatio = std::pow (2.0, (midiNoteNumber - sound->midiRootNote) / 12.0)
            * sound->sourceSampleRate / getSampleRate();
        
        sourceSamplePosition = 0.0;
        lgain = velocity;
        rgain = velocity;

        // Trigger ADSR
        adsr.noteOn();
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
    }
    else
    {
        clearCurrentNote();
        adsr.reset();
        pitchRatioMod = 0;
    }
}

void SourceSamplerVoice::pitchWheelMoved (int /*newValue*/) {}

void SourceSamplerVoice::controllerMoved (int /*controllerNumber*/, int /*newValue*/) {}

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

            if (sourceSamplePosition > playingSound->length)
            {
                stopNote (0.0f, false);
                pitchRatioMod = 0;
                break;
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
