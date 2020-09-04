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
                                        double attackTimeSecs,
                                        double releaseTimeSecs,
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

        params.attack  = static_cast<float> (attackTimeSecs);
        params.release = static_cast<float> (releaseTimeSecs);
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


// --------- implementation of SourceSamplerVoice


SourceSamplerVoice::SourceSamplerVoice() {}
SourceSamplerVoice::~SourceSamplerVoice() {}

bool SourceSamplerVoice::canPlaySound (SynthesiserSound* sound)
{
    return dynamic_cast<const SourceSamplerSound*> (sound) != nullptr;
}

void SourceSamplerVoice::startNote (int midiNoteNumber, float velocity, SynthesiserSound* s, int /*currentPitchWheelPosition*/)
{
    if (auto* sound = dynamic_cast<const SourceSamplerSound*> (s))
    {
        pitchRatio = std::pow (2.0, (midiNoteNumber - sound->midiRootNote) / 12.0)
                        * sound->sourceSampleRate / getSampleRate();

        sourceSamplePosition = 0.0;
        lgain = velocity;
        rgain = velocity;

        adsr.setSampleRate (sound->sourceSampleRate);
        adsr.setParameters (sound->params);

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
    // Aftertouch modifies the playback speed up to an octave
    pitchRatioMod = pitchRatio * (double)newAftertouchValue/127.0;
}


void SourceSamplerVoice::channelPressureChanged  (int newChannelPressureValue)
{
    // Channel aftertouch modifies the playback speed up to an octave
    pitchRatioMod = pitchRatio * (double)newChannelPressureValue/127.0;
}


//==============================================================================
void SourceSamplerVoice::renderNextBlock (AudioBuffer<float>& outputBuffer, int startSample, int numSamples)
{
    if (auto* playingSound = static_cast<SourceSamplerSound*> (getCurrentlyPlayingSound().get()))
    {
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
    }
}
