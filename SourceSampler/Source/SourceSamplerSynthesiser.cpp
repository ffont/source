/*
  ==============================================================================

    SourceSamplerVoice.cpp
    Created: 3 Sep 2020 2:13:36pm
    Author:  Frederic Font Corbera

  ==============================================================================
*/

#include "SourceSamplerSynthesiser.h"


SourceSamplerSynthesiser::SourceSamplerSynthesiser()
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

void SourceSamplerSynthesiser::setSamplerVoices(int nVoices)
{
    // Clear existing voices and re-create new ones
    clearVoices();
    for (auto i = 0; i < jmin(maxNumVoices, nVoices); ++i)
        addVoice (new SourceSamplerVoice);
    
    // Prepare newly created voices if processing specs are given (re-prepare voices)
    if (currentNumChannels > 0 ){
        // This is a big UGLY, might find a better fix for the future. The issue is that the sampler.prepare method (which in its turn
        // calls voices' prepare method) is only called once when plugin processor hits "prepareToPlay". Later on, if we remove and re-create
        // voices, these are never prepared and the tmpVoiceBuffer is never allocated (and processing chain is not prepared either). To
        // fix this, we store processing specs in the sampler object (num channels, sample rate, block size) so that everytime voice
        // objects are deleted and created, we can already prepare them. These specs are stored the first time "sampler.prepare" is called
        // because it is the first time sampler actually knows about what sample rate, block size and num channels shold work on.
        // HOWEVER, when we call "sampler.setSamplerVoices" the first time, the processing specs have not been stored yet, so we need to
        // add a check here to not call prepare on the voices with the default values of currentBlockSize, currentNumChannels and
        // getSampleRate() whcih are meaningless (=0) and will throw errors.
        for (auto* v : voices)
            dynamic_cast<SourceSamplerVoice*> (v)->prepare ({ getSampleRate(), (juce::uint32) currentBlockSize, (juce::uint32) currentNumChannels });
    }
    
    setNoteStealingEnabled (true);
}

void SourceSamplerSynthesiser::setReverbParameters (Reverb::Parameters params) {
    auto& reverb = fxChain.get<reverbIndex>();
    reverb.setParameters(params);
}

void SourceSamplerSynthesiser::prepare (const juce::dsp::ProcessSpec& spec) noexcept
{
    // Store current processing specs to be later used for voice processing and re-preparing
    currentNumChannels = spec.numChannels;
    currentBlockSize = spec.maximumBlockSize;
    setCurrentPlaybackSampleRate (spec.sampleRate);

    for (auto* v : voices)
        dynamic_cast<SourceSamplerVoice*> (v)->prepare (spec);

    fxChain.prepare (spec);
}

SourceSamplerSound* SourceSamplerSynthesiser::getSourceSamplerSoundByIdx (int idx) const noexcept {
    // Return the sound with the given idx (rember idx can be different than sound "child" number)
    for (int i=0; i<getNumSounds(); i++){
        auto* sound = static_cast<SourceSamplerSound*> (getSound(i).get());
        if (sound->getIdx() == idx){
            return sound;
        }
    }
    return nullptr;
}

void SourceSamplerSynthesiser::clearSourceSamplerSoundByIdx (int idx) {
    int indexInSamplerSoundsList = -1;
    for (int i=0; i<getNumSounds(); i++){
        auto* sound = static_cast<SourceSamplerSound*> (getSound(i).get());
        if (sound->getIdx() == idx){
            indexInSamplerSoundsList = i;
            break;
        }
    }
    if (indexInSamplerSoundsList > -1){
        removeSound(indexInSamplerSoundsList);
    }
}

SourceSamplerSound* SourceSamplerSynthesiser::getSourceSamplerSoundInPosition (int i) const noexcept {
    // Return the sound with the given idx (rember idx can be different than sound "child" number)
    if (i<getNumSounds()){
        return static_cast<SourceSamplerSound*> (getSound(i).get());
    }
    return nullptr;
}

//==============================================================================
ValueTree SourceSamplerSynthesiser::getState(){
    
    // NOTE: this collects sampeler's state but excludes individual sounds' state (see getStateForSound to get state of a sound)
    
    ValueTree state = ValueTree(STATE_SAMPLER);
    
    state.setProperty(STATE_PRESET_NUMVOICES, getNumVoices(), nullptr);
    
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

ValueTree SourceSamplerSynthesiser::getStateForSound(int idx){
    auto* sound = getSourceSamplerSoundByIdx(idx);  // This index corresponds to sound idx (ID) in the sampler, not to the sound position
    if (sound != nullptr){  // Safety check to make sure we don't try to access a sound that does not exist
        return sound->getState();
    }
    return ValueTree(STATE_SAMPLER_SOUND);  // Otherwise return empty ValueTree
}

void SourceSamplerSynthesiser::loadState(ValueTree samplerState){
    
    // NOTE: this loads the sampeler's state but excludes individual sounds' state (see loadStateForSound to load the state for a sound)
            
    // Load number of voices
    if (samplerState.hasProperty(STATE_PRESET_NUMVOICES)){
        int numVoices = (int)samplerState.getProperty(STATE_PRESET_NUMVOICES);
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

void SourceSamplerSynthesiser::loadStateForSound(int idx, ValueTree soundState){
    auto* sound = getSourceSamplerSoundByIdx(idx);  // This index corresponds to sound idx (ID) in the sampler, not to the sound position
    if (sound != nullptr){  // Safety check to make sure we don't try to access a sound that does not exist
        sound->loadState(soundState);
    }
}

void SourceSamplerSynthesiser::noteOn (const int midiChannel,
             const int midiNoteNumber,
             const float velocity)
{
    const ScopedLock sl (lock);
    for (auto* sound : sounds)
    {
        if (sound->appliesToNote (midiNoteNumber) && sound->appliesToChannel (midiChannel))
        {
            // If hitting a note that's still ringing, stop it first (it could be
            // still playing because of the sustain or sostenuto pedal).
            for (auto* voice : voices)
                if (voice->getCurrentlyPlayingNote() == midiNoteNumber && voice->isPlayingChannel (midiChannel))
                   stopVoice (voice, 1.0f, true);
            // NOTE: I commented this lines above so that several notes of the same sound can be played at the
            // same time. I'm not sure why the default JUCE implementation does not allow that. Maybe I'm
            // missing something important here?
            auto* voice = findFreeVoice (sound, midiChannel, midiNoteNumber, isNoteStealingEnabled());
            dynamic_cast<SourceSamplerVoice*>(voice)->setModWheelValue(lastModWheelValue);
            startVoice (voice, sound, midiChannel, midiNoteNumber, velocity);
        }
    }
}

void SourceSamplerSynthesiser::handleMidiEvent (const MidiMessage& m)
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
        int number = m.getControllerNumber();
        int value = m.getControllerValue();
        
        
        // Store last value for mod wheel (used when triggering new notes)
        if (number == 1){
            lastModWheelValue = value;
        }
        
        // Handle controller in active voices
        handleController (channel, number, value);
        
        // Check midi mappings for the loaded sounds and update parameters if needed
        for (auto* s : sounds)
        {
            if (auto* sound = dynamic_cast<SourceSamplerSound*> (s)){
                std::vector<MidiCCMapping> mappings = sound->getMidiMappingsForCcNumber(number);
                for (int i=0; i<mappings.size(); i++){
                    float normInputValue = (float)value/127.0;  // This goes from 0 to 1
                    float value = jmap(normInputValue, mappings[i].minRange, mappings[i].maxRange);
                    sound->setParameterByNameFloatNorm(mappings[i].parameterName, value);
                }
            }
        }
    }
    else if (m.isProgramChange())
    {
        int index = m.getProgramChangeNumber();  // Preset index, this is 0-based so MIDI value 0 will be also 0 here
        String actionMessage = String(ACTION_LOAD_PRESET) + ":" + (String)index;
        sendActionMessage(actionMessage);
    }
}

void SourceSamplerSynthesiser::renderVoices (AudioBuffer< float > &outputAudio, int startSample, int numSamples)
{
    Synthesiser::renderVoices (outputAudio, startSample, numSamples);
    auto block = juce::dsp::AudioBlock<float> (outputAudio);
    auto blockToUse = block.getSubBlock ((size_t) startSample, (size_t) numSamples);
    auto contextToUse = juce::dsp::ProcessContextReplacing<float> (blockToUse);
    fxChain.process (contextToUse);
}

