/*
  ==============================================================================

    SourceSamplerVoice.h
    Created: 23 Mar 2022 1:38:24pm
    Author:  Frederic Font Corbera

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>
#include "helpers_source.h"
#include "SourceSamplerSound.h"


class SourceSamplerVoice: public juce::SynthesiserVoice
{
public:
    //==============================================================================
    SourceSamplerVoice();

    ~SourceSamplerVoice() override;

    //==============================================================================
    bool canPlaySound (juce::SynthesiserSound*) override;
    
    void updateParametersFromSourceSamplerSound(SourceSamplerSound* sound);

    void startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int pitchWheel) override;
    void stopNote (float velocity, bool allowTailOff) override;

    void pitchWheelMoved (int newValue) override;
    void controllerMoved (int controllerNumber, int newValue) override;
    void aftertouchChanged (int newAftertouchValue) override;
    void channelPressureChanged  (int newChannelPressureValue) override;

    void renderNextBlock (juce::AudioBuffer<float>&, int startSample, int numSamples) override;
    using SynthesiserVoice::renderNextBlock;
    
    //==============================================================================
    void prepare (const juce::dsp::ProcessSpec& spec);
    
    float getPlayingPositionPercentage();  // Audio thread only (use getPublishedState from other threads)
    
    /** Voice state published by the audio thread (once per block, and when notes start/stop) so that other threads
        (e.g. the UI) can read it safely. sourceSound is only an identifier: it must not be dereferenced, compare it
        with the sounds owned by the caller instead. */
    struct PublishedState
    {
        bool active = false;
        const SourceSound* sourceSound = nullptr;
        float playPosition = -1.0f;  // Relative position (0-1) in the sound being played
    };
    PublishedState getPublishedState() const noexcept;
    int getNoteIndex(int midiNote);
    
    SourceSamplerSound* getCurrentlyPlayingSourceSamplerSound() const noexcept;
    
    void setModWheelValue(int newValue);
    
    //==============================================================================
    // MPE
    
    /** Called by the synth before starting a note with the MPE settings and the last values received on the note's channel. */
    void setMpeState (bool enabled, int perNotePitchBendRange, int initialPressure, int initialSlide, int masterPitchWheelValue);
    /** Pitch bend received on the MPE master channel (applies to all voices using the sound's pitch bend range). */
    void setMasterPitchBend (int pitchWheelValue);
    float getMpeSourceValue (SourceHelpers::MpeSource source) const noexcept;  // Normalised 0..1


private:
    /** Returns the value of a sound parameter for this voice: the sound's value, or the value set by an MPE mapping. */
    float getParam (SourceSamplerSound* sound, const juce::Identifier& identifier) const noexcept
    {
        for (int i = 0; i < numParameterOverrides; ++i)
            if (parameterOverrides[(size_t) i].first == identifier)
                return parameterOverrides[(size_t) i].second;
        return sound->getParameterFloat (identifier);
    }
    void updateMpeParameterOverrides (SourceSamplerSound* sound);
    void publishState (SourceSamplerSound* playingSound) noexcept;  // Audio thread, pass nullptr when the voice stops
    std::atomic<bool> publishedActive { false };
    std::atomic<const SourceSound*> publishedSourceSound { nullptr };
    std::atomic<float> publishedPlayPosition { -1.0f };
    double computePitchBendSemitones (SourceSamplerSound* sound, int pitchWheelValue);
    
    bool mpeMode = false;
    int mpePitchBendRange = SourceDefaults::mpePitchBendRange;
    float mpePress = 0.0f;   // 0..1
    float mpeSlide = 0.5f;   // 0..1
    float mpeGlide = 0.0f;   // -1..1
    int pendingMasterPitchWheelValue = 8192;
    double masterPitchBendSemitones = 0.0;
    SourceSound::MpeMappings mpeMappings;  // Copy of the sound's MPE mappings
    int numMpeMappings = 0;
    std::array<std::pair<juce::Identifier, float>, SourceSound::maxMpeMappings> parameterOverrides;
    int numParameterOverrides = 0;
    
    int pluginNumChannelsSize = 0;
    int currentlyPlayedNoteIndex = 0;
    
    int currentModWheelValue = 0; // Configured on noteONn and updated when modwheel is moved. This is needed to make modWheel modulationm persist across voices
    float currentNoteVelocity = 1.0;
    
    //==============================================================================
    // Sample reading and rendering
    bool playheadDirectionIsForward = true; // true = forward, false = backward
    float targetPlayheadSamplePosition = 0;  // only used in "freeze" mode
    double pitchRatio = 0;
    double pitchModSemitones = 0;  // For aftertouch, modulation wheel
    double pitchBendModSemitones = 0;
    double playheadSamplePosition = 0;
    double playheadSamplePositionMod = 0;
    float pan = 0;
    float lgain = 0, rgain = 0;
    juce::ADSR adsr;
    juce::ADSR adsrFilter;
    
    //==============================================================================
    // ProcessorChain (filter, pan and master gain)
    enum
    {
        filterIndex,
        masterGainIndex
    };
    juce::dsp::ProcessorChain<juce::dsp::LadderFilter<float>, juce::dsp::Gain<float>> processorChain;
    juce::AudioBuffer<float> tmpVoiceBuffer;  // used for processing voice contents in each process block, then adding to the main output buffer
    
    float filterCutoff = 20000.0f;
    float filterRessonance = 0.0f;
    float filterCutoffMod = 0.0f;  // For aftertouch, modulation wheel
    float gainMod = 1.0f;  // For aftertouch, modulation wheel
    int startPositionSample = 0;
    int endPositionSample = 0;
    int loopStartPositionSample = 0;
    int fixedLoopStartPositionSample = 0;
    int loopEndPositionSample = 0;
    int fixedLoopEndPositionSample = 0;
    bool hasNotYetPassedLoopStartPositionForTheFirstTime = true;
    
    // NOTE: the default values of the parameters above do not really matter because they'll be overriden by
    // the loaded sonund defaults
    
    juce::AudioBuffer<float> debugBuffer;  // Used only to store samples for debugging purposes
    int debugBufferCurrentPosition = 0;
    bool isRecordingToDebugBuffer = false;
    void startRecordingToDebugBuffer(int bufferSize);
    void writeToDebugBuffer(float sample);
    void endRecordingToDebugBuffer(juce::String outFilename);
    bool debugBufferFinishedRecording = false;
    
    JUCE_LEAK_DETECTOR (SourceSamplerVoice)
};
