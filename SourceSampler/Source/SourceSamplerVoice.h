/*
  ==============================================================================

    SourceSamplerVoice.h
    Created: 23 Mar 2022 1:38:24pm
    Author:  Frederic Font Corbera

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>
#include "defines.h"
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
    
    float getPlayingPositionPercentage();
    int getNoteIndex(int midiNote);
    
    SourceSamplerSound* getCurrentlyPlayingSourceSamplerSound() const noexcept;
    
    void setModWheelValue(int newValue);


private:
    int pluginNumChannelsSize = 0;
    int currentlyPlayedNoteIndex = 0;
    
    int currentModWheelValue = 0; // Configured on noteONn and updated when modwheel is moved. This is needed to make modWheel modulationm persist across voices
    
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
    float filterCutoffVelMod = 0.0f;  // For aftertouch, modulation wheel
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
