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


class SourceSamplerVoice: public juce::MPESynthesiserVoice
{
public:
    //==============================================================================
    SourceSamplerVoice();

    ~SourceSamplerVoice() override;

    //==============================================================================
    bool canPlaySound (juce::SynthesiserSound*);  // NOT NEEDED FOR MPE?
    
    void updateParametersFromSourceSamplerSound(SourceSamplerSound* sound);

    void startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int pitchWheel);
    void stopNote (float velocity, bool allowTailOff);
    
    void noteStopped (bool allowTailOff) override
    {
        jassert (currentlyPlayingNote.keyState == juce::MPENote::off);
        stopNote(0, allowTailOff);
    }
    
    void noteStarted() override
    {
        jassert (currentlyPlayingNote.isValid());
        jassert (currentlyPlayingNote.keyState == juce::MPENote::keyDown
                 || currentlyPlayingNote.keyState == juce::MPENote::keyDownAndSustained);

        startNote(currentlyPlayingNote.initialNote,
                  currentlyPlayingNote.noteOnVelocity.asUnsignedFloat(),
                  ,
                  currentlyPlayingNote.pitchbend.as14BitInt());
    }


    void pitchWheelMoved (int newValue);
    void controllerMoved (int controllerNumber, int newValue);
    void aftertouchChanged (int newAftertouchValue);
    void channelPressureChanged  (int newChannelPressureValue);

    void renderNextBlock (juce::AudioBuffer<float>&, int startSample, int numSamples) override;
    using juce::MPESynthesiserVoice::renderNextBlock;
    
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
