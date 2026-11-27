/*
  ==============================================================================

    SourceSamplerVoice.h
    Created: 3 Sep 2020 2:13:36pm
    Author:  Frederic Font Corbera

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "helpers_source.h"
#include "SourceSamplerVoice.h"
#include "SourceSamplerSound.h"
#include "SourceFxChain.h"
#include <array>



class SourceSamplerSynthesiser: public juce::Synthesiser
{
public:
    SourceSamplerSynthesiser();
    
    /** MIDI events received in the audio thread that need to be handled in the message thread because they
        modify the state (CC mappings, program changes). */
    struct ControlEvent
    {
        enum class Type { controller, programChange };
        Type type = Type::controller;
        int channel = 0;
        int number = 0;
        int value = 0;
    };
    
    /** Pops all pending control events and calls the given function for each of them. Call from the message thread. */
    template <typename Callback>
    void processPendingControlEvents (Callback&& callback)
    {
        const auto scope = controlEventsFifo.read (controlEventsFifo.getNumReady());
        scope.forEach ([&] (int index) { callback (controlEvents[(size_t) index]); });
    }
    
    /** The lock used by the synth while rendering/triggering voices. Hold it when modifying voices from another thread. */
    const juce::CriticalSection& getVoicesLock() const noexcept { return lock; }
    /** Returns a voice without taking the voices lock (which the audio thread holds while rendering), so the UI never
        waits for the audio thread. Only call from the message thread: voices are only added/removed there. */
    juce::SynthesiserVoice* getVoiceFromMessageThread (int index) const noexcept { return voices[index]; }
    
    void setGlobalMidiInChannel (int channel) noexcept { globalMidiInChannel = channel; }
    
    /** MPE (lower zone: master channel 1, member channels 2-16). When enabled, every sound responds to notes on any
        channel, and pitch bend, channel pressure and CC74 on member channels only affect the note on that channel. */
    void setMpeSettings (bool enabled, int perNotePitchBendRange) noexcept
    {
        mpePitchBendRange = juce::jlimit (1, 96, perNotePitchBendRange);
        mpeEnabled = enabled;
    }
    bool isMpeEnabled() const noexcept { return mpeEnabled.load(); }
    int getMpePitchBendRange() const noexcept { return mpePitchBendRange.load(); }
    int getGlobalMidiInChannel() const noexcept { return globalMidiInChannel.load(); }
    
    static constexpr auto maxNumVoices = 32;
    void setSamplerVoices(int nVoices);
    void prepare (const juce::dsp::ProcessSpec& spec) noexcept;
    
    //==============================================================================
    void noteOff (const int midiChannel,
                  const int midiNoteNumber,
                  const float velocity,
                  const bool allowTailOff) override;
    void noteOn (const int midiChannel,
                 const int midiNoteNumber,
                 const float velocity) override;
    void handleMidiEvent (const juce::MidiMessage& m) override;
    
    //==============================================================================
    void setReverbParameters (juce::Reverb::Parameters params);
    /** Effects parameters (see SourceFxChain). Can be called from any thread. */
    void setFxParameter (SourceFx::Param param, float value) noexcept { fxChain.setParameter (param, value); }
    float getFxParameter (SourceFx::Param param) const noexcept { return fxChain.getParameter (param); }
    
private:
    //==============================================================================
    void renderVoices (juce::AudioBuffer< float > &outputAudio, int startSample, int numSamples) override;
    void pushControlEvent (ControlEvent event) noexcept;  // Audio thread, lock free (events are dropped if FIFO is full)
    static constexpr int controlEventsFifoSize = 1024;
    juce::AbstractFifo controlEventsFifo { controlEventsFifoSize };
    std::array<ControlEvent, controlEventsFifoSize> controlEvents;
    std::atomic<int> globalMidiInChannel { 0 };
    std::atomic<bool> mpeEnabled { SourceDefaults::mpeEnabled };
    std::atomic<int> mpePitchBendRange { SourceDefaults::mpePitchBendRange };
    // Last per-channel MPE values (MPE controllers send these right before the note-on on the note's channel)
    std::array<int, 16> lastChannelPressure {};
    std::array<int, 16> lastSlide { 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64 };
    
    int lastModWheelValue = 0;
    int currentNumChannels = 0;
    int currentBlockSize = 0;
    SourceFxChain fxChain;
};
