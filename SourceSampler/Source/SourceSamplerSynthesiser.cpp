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
    juce::Reverb::Parameters reverbParameters;
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
    for (auto i = 0; i < juce::jmin(maxNumVoices, nVoices); ++i)
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

//==============================================================================

void SourceSamplerSynthesiser::noteOn (const int midiChannel,
             const int midiNoteNumber,
             const float velocity)
{
    const juce::ScopedLock sl (lock);
    int velocityInMidiRange = (int)std::round(127.0 * velocity);
    for (auto* sound : sounds)
    {
        if (sound->appliesToNote (midiNoteNumber) && dynamic_cast<SourceSamplerSound*>(sound)->appliesToVelocity(velocityInMidiRange) && sound->appliesToChannel (midiChannel))
        {
            //DBG(midiNoteNumber << "," << velocityInMidiRange << ": " << dynamic_cast<SourceSamplerSound*>(sound)->getSoundName());
            // If hitting a note that's still ringing, stop it first (it could be
            // still playing because of the sustain or sostenuto pedal).
            for (auto* voice : voices)
                if (voice->getCurrentlyPlayingNote() == midiNoteNumber && voice->isPlayingChannel (midiChannel))
                   stopVoice (voice, 1.0f, true);
            auto* voice = findFreeVoice (sound, midiChannel, midiNoteNumber, isNoteStealingEnabled());
            dynamic_cast<SourceSamplerVoice*>(voice)->setModWheelValue(lastModWheelValue);
            startVoice (voice, sound, midiChannel, midiNoteNumber, velocity);
        }
    }
}

void SourceSamplerSynthesiser::handleMidiEvent (const juce::MidiMessage& m)
{
    const int channel = m.getChannel();
    
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
                int globalMidiChannel = sound->getSourceSound()->getGlobalContext().midiInChannel;
                int soundMidiChannel = sound->getParameterInt(IDs::midiChannel);
                bool appliesToChannel = false;
                if (soundMidiChannel == 0){
                    // Check with global
                    appliesToChannel = (globalMidiChannel == 0) || (globalMidiChannel == channel);
                } else {
                    // Check with sound channel
                    appliesToChannel = soundMidiChannel == channel;
                }
                if (appliesToChannel){
                    std::vector<MidiCCMapping*> mappings = sound->getSourceSound()->getMidiMappingsForCcNumber(number);
                    for (int i=0; i<mappings.size(); i++){
                        float normInputValue = (float)value/127.0;  // This goes from 0 to 1
                        float value = juce::jmap(normInputValue, mappings[i]->minRange.get(), mappings[i]->maxRange.get());
                        sound->getSourceSound()->setParameterByNameFloat(mappings[i]->parameterName.get(), value, true);
                    }
                }
            }
        }
    }
    else if (m.isProgramChange())
    {
        int index = m.getProgramChangeNumber();  // Preset index, this is 0-based so MIDI value 0 will be also 0 here
        juce::String actionMessage = juce::String(ACTION_LOAD_PRESET) + ":" + (juce::String)index;
        sendActionMessage(actionMessage);
    }
}

void SourceSamplerSynthesiser::renderVoices (juce::AudioBuffer< float > &outputAudio, int startSample, int numSamples)
{
    Synthesiser::renderVoices (outputAudio, startSample, numSamples);
    auto block = juce::dsp::AudioBlock<float> (outputAudio);
    auto blockToUse = block.getSubBlock ((size_t) startSample, (size_t) numSamples);
    auto contextToUse = juce::dsp::ProcessContextReplacing<float> (blockToUse);
    fxChain.process (contextToUse);
}

//==============================================================================

void SourceSamplerSynthesiser::setReverbParameters (juce::Reverb::Parameters params) {
    auto& reverb = fxChain.get<reverbIndex>();
    reverb.setParameters(params);
}
