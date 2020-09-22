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
        @param maxSampleLengthSeconds   a maximum length of audio to read from the audio
                                        source, in seconds
        @param _pluginSampleRate  sample rate of the host plugin
        @param _pluginBlockSize  block size of the host plugin
    */
    SourceSamplerSound (int _idx,
                        const String& name,
                        AudioFormatReader& source,
                        double maxSampleLengthSeconds,
                        double _pluginSampleRate,
                        int _pluginBlockSize);

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
    bool appliesToNote (int midiNoteNumber) override;
    bool appliesToChannel (int midiChannel) override;
    
    //==============================================================================
    void setParameterByNameFloat(const String& name, float value);
    void setParameterByNameInt(const String& name, int value);
    
    //==============================================================================
    ValueTree getState();
    void loadState(ValueTree soundState);
    
    //==============================================================================
    int getLengthInSamples();
    float getLengthInSeconds();
    int getIdx();
    float getPlayingPositionPercentage();

private:
    //==============================================================================
    friend class SourceSamplerVoice;
    
    int idx = -1;  // This is the idx of the sound in the loadedSoundsInfo ValueTree stored in the plugin processor

    String name;
    std::unique_ptr<AudioBuffer<float>> data;
    double sourceSampleRate;
    BigInteger midiNotes;
    int length = 0;
    
    double pluginSampleRate;
    int pluginBlockSize;

    // Define sound "controllable" parameters here
    
    // --> Start auto-generated code A
    float filterCutoff = 20000.0f;
    float filterRessonance = 0.0f;
    float maxPitchRatioMod = 0.1f;
    float maxFilterCutoffMod = 10.0f;
    float gain = -10.0f;
    ADSR::Parameters ampADSR = {0.01f, 0.0f, 1.0f, 1.0f};
    ADSR::Parameters filterADSR = {0.01f, 0.0f, 1.0f, 1.0f};
    float maxFilterADSRMod = 1.0f;
    int midiRootNote = 64;
    float basePitch = 0.0f;
    float startPosition = 0.0f;
    float endPosition = 1.0f;
    float loopStartPosition = 0.0f;
    float loopEndPosition = 1.0f;
    int loopXFadeNSamples = 500;
    int launchMode = 0;
    int reverse = 0;
    float maxGainVelMod = 0.5f;
    float pan = 0.0f;
    float pitchBendRangeUp = 12.0f;
    float pitchBendRangeDown = 12.0f;
    // --> End auto-generated code A

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
    
    float getPlayingPositionPercentage();
    
    SourceSamplerSound* getCurrentlyPlayingSourceSamplerSound() const noexcept;


private:
    //==============================================================================
    // Sample reading and rendering
    double pitchRatio = 0;
    double pitchRatioMod = 0;  // For aftertouch, modulation wheel
    double pitchBendModSemitones = 0;
    double sourceSamplePosition = 0;
    float pan = 0;
    float lgain = 0, rgain = 0;
    ADSR adsr;
    ADSR adsrFilter;
    
    //==============================================================================
    // ProcessorChain (filter, pan and master gain)
    enum
    {
        filterIndex,
        masterGainIndex
    };
    juce::dsp::ProcessorChain<juce::dsp::LadderFilter<float>, juce::dsp::Gain<float>> processorChain;
    float filterCutoff = 20000.0f;
    float filterRessonance = 0.0f;
    float filterCutoffMod = 0.0f;
    float masterGain = 0.5f;
    int startPositionSample = 0;
    int endPositionSample = 0;
    int loopStartPositionSample = 0;
    int fixedLoopStartPositionSample = 0;
    int loopEndPositionSample = 0;
    int fixedLoopEndPositionSample = 0;
    bool hasNotYetPassedLoopStartPositionForTheFirstTime = true;
    
    // NOTE: the default values of the parameters above do not really matter because they'll be overriden by
    // the loaded sonund defaults
    
    
    AudioBuffer<float> debugBuffer;  // Used only to store samples for debugging purposes
    int debugBufferCurrentPosition = 0;
    bool isRecordingToDebugBuffer = false;
    void startRecordingToDebugBuffer(int bufferSize);
    void writeToDebugBuffer(float sample);
    void endRecordingToDebugBuffer(String outFilename);
    bool debugBufferFinishedRecording = false;
    
    JUCE_LEAK_DETECTOR (SourceSamplerVoice)
};


class SourceSamplerSynthesiser: public Synthesiser,
                                public ActionBroadcaster
{
public:
    static constexpr auto maxNumVoices = 32;
    
    SourceSamplerSynthesiser()
    {
        setSamplerVoices(maxNumVoices);
        
        // Configure effects chain
        Reverb::Parameters reverbParameters;
        reverbParameters.roomSize = 0.5f;
        reverbParameters.damping = 0.5f;
        reverbParameters.wetLevel = 0.0f;
        reverbParameters.dryLevel = 1.0f;
        reverbParameters.width = 1.0f;
        reverbParameters.freezeMode = 0.0f;
        auto& reverb = fxChain.get<reverbIndex>();
        reverb.setParameters(reverbParameters);
    }
    
    void setSamplerVoices(int nVoices)
    {
        clearVoices();
        
        for (auto i = 0; i < jmin(maxNumVoices, nVoices); ++i)
            addVoice (new SourceSamplerVoice);
        
        setNoteStealingEnabled (true);
    }
    
    void setReverbParameters (Reverb::Parameters params) {
        auto& reverb = fxChain.get<reverbIndex>();
        reverb.setParameters(params);
    }
    
    void prepare (const juce::dsp::ProcessSpec& spec) noexcept
    {
        setCurrentPlaybackSampleRate (spec.sampleRate);

        for (auto* v : voices)
            dynamic_cast<SourceSamplerVoice*> (v)->prepare (spec);

        fxChain.prepare (spec);
    }
    
    SourceSamplerSound* getSourceSamplerSoundByIdx (int idx) const noexcept {
        // Return the sound with the given idx (rember idx can be different than sound "child" number)
        for (int i=0; i<getNumSounds(); i++){
            auto* sound = static_cast<SourceSamplerSound*> (getSound(i).get());
            if (sound->getIdx() == idx){
                return sound;
            }
        }
        return nullptr;
    }
    
    SourceSamplerSound* getSourceSamplerSoundInPosition (int i) const noexcept {
        // Return the sound with the given idx (rember idx can be different than sound "child" number)
        if (i<getNumSounds()){
            return static_cast<SourceSamplerSound*> (getSound(i).get());
        }
        return nullptr;
    }
    
    //==============================================================================
    ValueTree getState(){
        
        // NOTE: this collects sampeler's state but excludes individual sounds' state (see getStateForSound to get state of a sound)
        
        ValueTree state = ValueTree(STATE_SAMPLER);
        
        state.setProperty(STATE_NUMVOICES, getNumVoices(), nullptr);
        
        // Add reverb settings to state
        ValueTree reverbParameters = ValueTree(STATE_REVERB_PARAMETERS);
        auto& reverb = fxChain.get<reverbIndex>();
        Reverb::Parameters params = reverb.getParameters();
        reverbParameters.setProperty(STATE_REVERB_ROOMSIZE, params.roomSize, nullptr);
        reverbParameters.setProperty(STATE_REVERB_DAMPING, params.damping, nullptr);
        reverbParameters.setProperty(STATE_REVERB_WETLEVEL, params.wetLevel, nullptr);
        reverbParameters.setProperty(STATE_REVERB_DRYLEVEL, params.dryLevel, nullptr);
        reverbParameters.setProperty(STATE_REVERB_WIDTH, params.width, nullptr);
        reverbParameters.setProperty(STATE_REVERB_FREEZEMODE, params.freezeMode, nullptr);
        state.appendChild(reverbParameters, nullptr);
        
        return state;
    }
    
    ValueTree getStateForSound(int idx){
        auto* sound = getSourceSamplerSoundByIdx(idx);  // This index corresponds to sound idx (ID) in the sampler, not to the sound position
        if (sound != nullptr){  // Safety check to make sure we don't try to access a sound that does not exist
            return sound->getState();
        }
        return ValueTree();  // Otherwise return empty ValueTree
    }
    
    void loadState(ValueTree samplerState){
        
        // NOTE: this loads the sampeler's state but excludes individual sounds' state (see loadStateForSound to load the state for a sound)
                
        // Load number of voices
        if (samplerState.hasProperty(STATE_NUMVOICES)){
            int numVoices = (int)samplerState.getProperty(STATE_NUMVOICES);
            setSamplerVoices(numVoices);
        }
        
        // Load reverb parameters
        ValueTree reveberParametersVT = samplerState.getChildWithName(STATE_REVERB_PARAMETERS);
        if (reveberParametersVT.isValid()){
            Reverb::Parameters reverbParameters;
            if (reveberParametersVT.hasProperty(STATE_REVERB_ROOMSIZE)){
                reverbParameters.roomSize = (float)reveberParametersVT.getProperty(STATE_REVERB_ROOMSIZE);
            }
            if (reveberParametersVT.hasProperty(STATE_REVERB_DAMPING)){
                reverbParameters.damping = (float)reveberParametersVT.getProperty(STATE_REVERB_DAMPING);
            }
            if (reveberParametersVT.hasProperty(STATE_REVERB_WETLEVEL)){
                reverbParameters.wetLevel = (float)reveberParametersVT.getProperty(STATE_REVERB_WETLEVEL);
            }
            if (reveberParametersVT.hasProperty(STATE_REVERB_DRYLEVEL)){
                reverbParameters.dryLevel = (float)reveberParametersVT.getProperty(STATE_REVERB_DRYLEVEL);
            }
            if (reveberParametersVT.hasProperty(STATE_REVERB_WIDTH)){
                reverbParameters.width = (float)reveberParametersVT.getProperty(STATE_REVERB_WIDTH);
            }
            if (reveberParametersVT.hasProperty(STATE_REVERB_FREEZEMODE)){
                reverbParameters.freezeMode = (float)reveberParametersVT.getProperty(STATE_REVERB_FREEZEMODE);
            }
            auto& reverb = fxChain.get<reverbIndex>();
            reverb.setParameters(reverbParameters);
        }
    }
    
    void loadStateForSound(int idx, ValueTree soundState){
        auto* sound = getSourceSamplerSoundByIdx(idx);  // This index corresponds to sound idx (ID) in the sampler, not to the sound position
        if (sound != nullptr){  // Safety check to make sure we don't try to access a sound that does not exist
            sound->loadState(soundState);
        }
    }
    
    void handleMidiEvent (const MidiMessage& m) override
    {
        const int channel = m.getChannel();
        
        if ((midiInChannel > 0) && (channel != midiInChannel)){
            // If there is midi channel filter and it is not matched, return
            return;
        }
        
        if (m.isNoteOn())
        {
            noteOn (channel, m.getNoteNumber(), m.getFloatVelocity());
        }
        else if (m.isNoteOff())
        {
            noteOff (channel, m.getNoteNumber(), m.getFloatVelocity(), true);
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff())
        {
            allNotesOff (channel, true);
        }
        else if (m.isPitchWheel())
        {
            const int wheelPos = m.getPitchWheelValue();
            lastPitchWheelValues [channel - 1] = wheelPos;
            handlePitchWheel (channel, wheelPos);
        }
        else if (m.isAftertouch())
        {
            handleAftertouch (channel, m.getNoteNumber(), m.getAfterTouchValue());
        }
        else if (m.isChannelPressure())
        {
            handleChannelPressure (channel, m.getChannelPressureValue());
        }
        else if (m.isController())
        {
            handleController (channel, m.getControllerNumber(), m.getControllerValue());
        }
        else if (m.isProgramChange())
        {
            int index = m.getProgramChangeNumber();  // Preset index, this is 0-based so MIDI value 0 will be also 0 here
            String actionMessage = String(ACTION_LOAD_PRESET) + ":" + (String)index;
            sendActionMessage(actionMessage);
        }
    }
    
    int midiInChannel = 0; // 0 all channels, 1=channel 1, 2=channel 2 etc
    
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
