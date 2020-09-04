/*
  ==============================================================================

    SourceSamplerVoice.h
    Created: 3 Sep 2020 2:13:36pm
    Author:  Frederic Font Corbera

  ==============================================================================
*/

#pragma once

#include "JuceHeader.h"
#include "defines.h"


class SourceSamplerSound: public SynthesiserSound
{
public:
    //==============================================================================
    /** Creates a sampled sound from an audio reader.

        This will attempt to load the audio from the source into memory and store
        it in this object.

        @param name         a name for the sample
        @param source       the audio to load. This object can be safely deleted by the
                            caller after this constructor returns
        @param midiNotes    the set of midi keys that this sound should be played on. This
                            is used by the SynthesiserSound::appliesToNote() method
        @param midiNoteForNormalPitch   the midi note at which the sample should be played
                                        with its natural rate. All other notes will be pitched
                                        up or down relative to this one
        @param attackTimeSecs   the attack (fade-in) time, in seconds
        @param releaseTimeSecs  the decay (fade-out) time, in seconds
        @param maxSampleLengthSeconds   a maximum length of audio to read from the audio
                                        source, in seconds
    */
    SourceSamplerSound (const String& name,
                        AudioFormatReader& source,
                        const BigInteger& midiNotes,
                        int midiNoteForNormalPitch,
                        double attackTimeSecs,
                        double releaseTimeSecs,
                        double maxSampleLengthSeconds);

    /** Destructor. */
    ~SourceSamplerSound() override;

    //==============================================================================
    /** Returns the sample's name */
    const String& getName() const noexcept                  { return name; }

    /** Returns the audio sample data.
        This could return nullptr if there was a problem loading the data.
    */
    AudioBuffer<float>* getAudioData() const noexcept       { return data.get(); }

    //==============================================================================
    /** Changes the parameters of the ADSR envelope which will be applied to the sample. */
    void setEnvelopeParameters (ADSR::Parameters parametersToUse)    { params = parametersToUse; }

    //==============================================================================
    bool appliesToNote (int midiNoteNumber) override;
    bool appliesToChannel (int midiChannel) override;
    
    //==============================================================================
    void setParameterByNameFloat(const String& name, float value){
        // TODO: is there a way to find variable by string name?
        if (name == "filterCutoff") {
            filterCutoff = value;
        } else if (name == "filterRessonance") {
            filterRessonance = value;
        } else if (name == "maxPitchRatioMod") {
            maxPitchRatioMod = value;
        } else if (name == "maxFilterCutoffMod") {
            maxFilterCutoffMod = value;
        } else if (name == "gain") {
            gain = value;
        }
    }

private:
    //==============================================================================
    friend class SourceSamplerVoice;

    String name;
    std::unique_ptr<AudioBuffer<float>> data;
    double sourceSampleRate;
    BigInteger midiNotes;
    int length = 0, midiRootNote = 0;

    ADSR::Parameters params;
    
    float filterCutoff = 20000.0;  // Default cutoff (fully open)
    float filterRessonance = 0.0;  // Default resonance
    float maxPitchRatioMod = 0.1;  // 100% of current pitch ratio (1 octave)
    float maxFilterCutoffMod = 10.0; // 10 times the base cutoff
    float gain = 1.0; // Master gain for the sound

    JUCE_LEAK_DETECTOR (SourceSamplerSound)
};


class SourceSamplerVoice: public SynthesiserVoice
{
public:
    //==============================================================================
    /** Creates a SamplerVoice. */
    SourceSamplerVoice();

    /** Destructor. */
    ~SourceSamplerVoice() override;

    //==============================================================================
    bool canPlaySound (SynthesiserSound*) override;
    
    void updateParametersFromSourceSamplerSound(SourceSamplerSound* sound);

    void startNote (int midiNoteNumber, float velocity, SynthesiserSound*, int pitchWheel) override;
    void stopNote (float velocity, bool allowTailOff) override;

    void pitchWheelMoved (int newValue) override;
    void controllerMoved (int controllerNumber, int newValue) override;
    void aftertouchChanged (int newAftertouchValue) override;
    void channelPressureChanged  (int newChannelPressureValue) override;

    void renderNextBlock (AudioBuffer<float>&, int startSample, int numSamples) override;
    using SynthesiserVoice::renderNextBlock;
    
    //==============================================================================
    void prepare (const juce::dsp::ProcessSpec& spec);

private:
    //==============================================================================
    // Sample reading and rendering
    double pitchRatio = 0;
    double pitchRatioMod = 0;
    double sourceSamplePosition = 0;
    float lgain = 0, rgain = 0;
    ADSR adsr;
    
    //==============================================================================
    // ProcessorChain (filter and master gain)
    enum
    {
        filterIndex,
        masterGainIndex
    };
    juce::dsp::ProcessorChain<juce::dsp::LadderFilter<float>, juce::dsp::Gain<float>> processorChain;
    float filterCutoff = 20000.0f;
    float filterRessonance = 0.0f;
    float filterCutoffMod = 0.0f;
    float masterGain = 1.0f;
    
    // NOTE: the default values of the parameters above do not really matter because they'll be overriden by
    // the loaded sonund defaults

    JUCE_LEAK_DETECTOR (SourceSamplerVoice)
};


class SourceSamplerSynthesiser: public Synthesiser
{
public:
    static constexpr auto maxNumVoices = 16;
    
    SourceSamplerSynthesiser()
    {
        for (auto i = 0; i < maxNumVoices; ++i)
            addVoice (new SourceSamplerVoice);

        setNoteStealingEnabled (true);
    }
    
    void prepare (const juce::dsp::ProcessSpec& spec) noexcept
    {
        setCurrentPlaybackSampleRate (spec.sampleRate);

        for (auto* v : voices)
            dynamic_cast<SourceSamplerVoice*> (v)->prepare (spec);

        fxChain.prepare (spec);
    }
    
private:
    //==============================================================================
    void renderVoices (AudioBuffer< float > &outputAudio, int startSample, int numSamples) override
    {        
        Synthesiser::renderVoices (outputAudio, startSample, numSamples);

        auto block = juce::dsp::AudioBlock<float> (outputAudio);
        auto blockToUse = block.getSubBlock ((size_t) startSample, (size_t) numSamples);
        auto contextToUse = juce::dsp::ProcessContextReplacing<float> (blockToUse);
        fxChain.process (contextToUse);
    }

    enum
    {
        reverbIndex
    };

    juce::dsp::ProcessorChain<juce::dsp::Reverb> fxChain;
};
