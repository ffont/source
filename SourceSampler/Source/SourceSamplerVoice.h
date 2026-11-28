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

    //==============================================================================
    // Modulators
    
    /** History of the output of each modulator of the sound being played, written by the audio thread at
        modulatorHistoryRate (points per second) while the voice plays a sound with modulators. Read it from the
        message thread: positions in [count - modulatorHistorySize / 2, count) are safe to read. */
    static constexpr int modulatorHistorySize = 256;
    static constexpr double modulatorHistoryRate = 200.0;
    uint32_t getModulatorHistoryCount() const noexcept { return modulatorHistoryCount.load (std::memory_order_acquire); }
    float getModulatorHistoryValue (int modulatorIndex, uint32_t position) const noexcept
    {
        return modulatorHistory[(size_t) juce::jlimit (0, SourceSound::maxModulators - 1, modulatorIndex)][position % modulatorHistorySize].load (std::memory_order_relaxed);
    }
    /** Incremented each time a note starts (so readers know when a new note started in this voice). */
    uint32_t getModulatorNoteCounter() const noexcept { return modulatorNoteCounter.load (std::memory_order_acquire); }
    /** Current value of each MPE mapping of the sound for this voice (normalised parameter value, 0..1), published by
        the audio thread. getPublishedNumMpeValues() is 0 when MPE is off. */
    int getPublishedNumMpeValues() const noexcept { return publishedNumMpeValues.load (std::memory_order_acquire); }
    float getPublishedMpeValue (int index) const noexcept
    {
        return publishedMpeValues[(size_t) juce::jlimit (0, SourceSound::maxMpeMappings - 1, index)].load (std::memory_order_relaxed);
    }
    /** Size of the chunks in which voices playing sounds with modulators are rendered (modulators are updated once per chunk). */
    static constexpr int modulatorChunkSize = 32;


private:
    /** Returns the value of a sound parameter for this voice: the sound's value, or the value set by an MPE mapping,
        plus the offsets of the modulators that target the parameter. */
    float getParam (SourceSamplerSound* sound, const juce::Identifier& identifier) const noexcept
    {
        float value = 0.0f;
        bool overridden = false;
        for (int i = 0; i < numParameterOverrides; ++i)
        {
            if (parameterOverrides[(size_t) i].first == identifier)
            {
                value = parameterOverrides[(size_t) i].second;
                overridden = true;
                break;
            }
        }
        if (! overridden)
            value = sound->getParameterFloat (identifier);
        for (int i = 0; i < numModulationOffsets; ++i)
        {
            const auto& offset = modulationOffsets[(size_t) i];
            if (offset.parameter == identifier)
                return SourceHelpers::applyModulationOffset (value, offset.offset, offset.parameterMin, offset.parameterMax);
        }
        return value;
    }
    
    void renderChunk (juce::AudioBuffer<float>&, int startSample, int numSamples);
    /** Computes the modulators' outputs at the current time (and the resulting parameter offsets), then advances them
        by numSamples. Audio thread. */
    void updateModulators (SourceSamplerSound* sound, int numSamples);
    void resetModulators (SourceSamplerSound* sound);
    struct ModulatorVoiceState
    {
        double phase = 0.0;
        double elapsedSeconds = 0.0;  // Since the note started (LFO attack)
        float randomValue = 0.5f;
        SourceHelpers::ModulatorEnvelope envelope;
        float output = 0.0f;
    };
    struct ModulationOffset
    {
        juce::Identifier parameter;
        float offset = 0.0f;
        float parameterMin = 0.0f, parameterMax = 1.0f;
    };
    SourceSound::Modulators modulators;  // Copy of the sound's modulators
    int numModulators = 0;
    std::array<ModulatorVoiceState, SourceSound::maxModulators> modulatorStates;
    std::array<ModulationOffset, SourceSound::maxModulators> modulationOffsets;
    int numModulationOffsets = 0;
    double modulatorSampleRate = 44100.0;
    double samplesSinceLastHistoryPoint = 0.0;
    juce::Random modulatorRandom;
    std::array<std::array<std::atomic<float>, modulatorHistorySize>, SourceSound::maxModulators> modulatorHistory {};
    std::atomic<uint32_t> modulatorHistoryCount { 0 };
    std::atomic<uint32_t> modulatorNoteCounter { 0 };
    std::array<std::atomic<float>, SourceSound::maxMpeMappings> publishedMpeValues {};
    std::atomic<int> publishedNumMpeValues { 0 };
    void updateMpeParameterOverrides (SourceSamplerSound* sound);
    void publishState (SourceSamplerSound* playingSound) noexcept;  // Audio thread, pass nullptr when the voice stops
    std::atomic<bool> publishedActive { false };
    std::atomic<const SourceSound*> publishedSourceSound { nullptr };
    std::atomic<float> publishedPlayPosition { -1.0f };
    double computePitchBendSemitones (SourceSamplerSound* sound, int pitchWheelValue);
    
    bool mpeMode = false;
    int mpePitchBendRange = SourceDefaults::mpePitchBendRange;
    float mpePress = 0.0f;   // 0..1
    float currentPressure = 0.0f;  // Aftertouch / channel pressure of the note (0..1), for aftertouch modulators
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
