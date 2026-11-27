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
    setSamplerVoices(SourceDefaults::numVoices);
}

void SourceSamplerSynthesiser::setSamplerVoices(int nVoices)
{
    // Create and prepare the new voices before adding them to the synth, so the audio thread never sees an unprepared voice.
    // Processing specs are stored the first time "prepare" is called; before that voices will be prepared in "prepare".
    std::vector<SourceSamplerVoice*> newVoices;
    for (auto i = 0; i < juce::jlimit(1, (int)maxNumVoices, nVoices); ++i){
        auto* voice = new SourceSamplerVoice;
        if (currentNumChannels > 0)
            voice->prepare ({ getSampleRate(), (juce::uint32) currentBlockSize, (juce::uint32) currentNumChannels });
        newVoices.push_back (voice);
    }
    
    {
        const juce::ScopedLock sl (lock);
        clearVoices();
        for (auto* voice : newVoices)
            addVoice (voice);
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
            auto* sourceSound = static_cast<SourceSamplerSound*>(sound)->getSourceSound();
            for (auto* voice : voices){
                if (voice->getCurrentlyPlayingNote() == midiNoteNumber && voice->isPlayingChannel (midiChannel)){
                    auto* playingSound = static_cast<SourceSamplerVoice*>(voice)->getCurrentlyPlayingSourceSamplerSound();
                    if (playingSound != nullptr && playingSound->getSourceSound() == sourceSound)
                        stopVoice (voice, 1.0f, true);  // Only allow one single instance of SourceSamplerSound type per voice
                }
            }
            if (auto* voice = findFreeVoice (sound, midiChannel, midiNoteNumber, isNoteStealingEnabled())){
                auto* sourceVoice = static_cast<SourceSamplerVoice*>(voice);
                sourceVoice->setModWheelValue(lastModWheelValue);
                const int channelIndex = juce::jlimit (0, 15, midiChannel - 1);
                sourceVoice->setMpeState(mpeEnabled.load(), mpePitchBendRange.load(), lastChannelPressure[(size_t)channelIndex],
                                         lastSlide[(size_t)channelIndex], lastPitchWheelValues[MPE_MASTER_CHANNEL - 1]);
                startVoice (voice, sound, midiChannel, midiNoteNumber, velocity);
            }
        }
    }
}

void SourceSamplerSynthesiser::noteOff (const int midiChannel, const int midiNoteNumber, const float velocity, const bool allowTailOff)
{
    if (mpeEnabled.load() && midiChannel != MPE_MASTER_CHANNEL){
        // Per-note pressure ends with the note (the next note on this channel will send its own initial value)
        lastChannelPressure[(size_t) juce::jlimit (0, 15, midiChannel - 1)] = 0;
    }
    Synthesiser::noteOff (midiChannel, midiNoteNumber, velocity, allowTailOff);
}

void SourceSamplerSynthesiser::handleMidiEvent (const juce::MidiMessage& m)
{
    const int channel = m.getChannel();
    const int channelIndex = juce::jlimit (0, 15, channel - 1);
    const bool mpe = mpeEnabled.load();
    const bool isMasterChannelInMpe = mpe && channel == MPE_MASTER_CHANNEL;
    
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
        if (isMasterChannelInMpe){
            // Master channel pitch bend affects all notes (with each sound's pitch bend range)
            const juce::ScopedLock sl (lock);
            for (auto* voice : voices)
                static_cast<SourceSamplerVoice*>(voice)->setMasterPitchBend (wheelPos);
        } else {
            // In MPE mode this is per-note pitch bend (only the voice playing on this channel)
            handlePitchWheel (channel, wheelPos);
        }
    }
    else if (m.isAftertouch())
    {
        handleAftertouch (channel, m.getNoteNumber(), m.getAfterTouchValue());
    }
    else if (m.isChannelPressure())
    {
        lastChannelPressure[(size_t) channelIndex] = m.getChannelPressureValue();
        if (isMasterChannelInMpe){
            const juce::ScopedLock sl (lock);
            for (auto* voice : voices)
                voice->channelPressureChanged (m.getChannelPressureValue());
        } else {
            handleChannelPressure (channel, m.getChannelPressureValue());
        }
    }
    else if (m.isController())
    {
        int number = m.getControllerNumber();
        int value = m.getControllerValue();
        
        // Store last value for mod wheel (used when triggering new notes)
        if (number == 1 && (!mpe || channel == MPE_MASTER_CHANNEL)){
            lastModWheelValue = value;
        }
        
        if (number == MPE_SLIDE_CC)
            lastSlide[(size_t) channelIndex] = value;
        
        // Handle controller in active voices (in MPE mode, controllers on the master channel affect all voices)
        if (isMasterChannelInMpe){
            const juce::ScopedLock sl (lock);
            for (auto* voice : voices)
                voice->controllerMoved (number, value);
        } else {
            handleController (channel, number, value);
        }
        
        // MIDI CC mappings modify sound parameters (which live in the state), so these are applied in the message thread.
        // In MPE mode, controllers on member channels are per-note expression and don't trigger CC mappings.
        if (!mpe || channel == MPE_MASTER_CHANNEL)
            pushControlEvent ({ ControlEvent::Type::controller, channel, number, value });
    }
    else if (m.isProgramChange())
    {
        // Preset index, this is 0-based so MIDI value 0 will be also 0 here. Presets are loaded in the message thread.
        pushControlEvent ({ ControlEvent::Type::programChange, channel, m.getProgramChangeNumber(), 0 });
    }
}

void SourceSamplerSynthesiser::pushControlEvent (ControlEvent event) noexcept
{
    const auto scope = controlEventsFifo.write (1);
    scope.forEach ([&] (int index) { controlEvents[(size_t) index] = event; });
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
    fxChain.setReverbParameters(params);  // Applied in the audio thread (with dry level compensation, see SourceFxChain)
}
