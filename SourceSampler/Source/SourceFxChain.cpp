/*
  ==============================================================================

    SourceFxChain.cpp

  ==============================================================================
*/

#include "SourceFxChain.h"

using SourceFx::Param;


SourceFxChain::SourceFxChain()
{
    for (size_t i = 0; i < parameters.size(); ++i)
    {
        parameters[i].store (SourceFx::getParamInfos()[i].defaultValue);
        currentValues[i] = parameters[i].load();
    }
    juce::Reverb::Parameters reverbParameters;
    reverbParameters.roomSize = 0.5f;
    reverbParameters.damping = 0.5f;
    reverbParameters.wetLevel = 0.0f;
    reverbParameters.dryLevel = 1.0f;
    reverbParameters.width = 1.0f;
    reverbParameters.freezeMode = 0.0f;
    setReverbParameters (reverbParameters);
}

void SourceFxChain::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    numChannels = (int) spec.numChannels;
    delayTimeSamples.reset (sampleRate, 0.08);
    for (auto* smoothed : { &driveGain, &driveOutputGain, &driveMix })
        smoothed->reset (sampleRate, 0.03);

    // Apply the parameters before preparing the processors, so they start with their final values (no fades)
    firstUpdate = true;
    updateProcessors();

    chorus.prepare (spec);
    phaser.prepare (spec);
    reverb.prepare (spec);
    compressor.prepare (spec);
    compressorMakeup.prepare (spec);
    compressorMakeup.setRampDurationSeconds (0.05);

    // The delay processes (up to) two channels
    delayLine.setMaximumDelayInSamples ((int) std::ceil (maxDelayTimeMs * 0.001 * sampleRate) + 4);
    delayLine.prepare ({ spec.sampleRate, spec.maximumBlockSize, (juce::uint32) juce::jmin (2, numChannels) });
    reset();
}

void SourceFxChain::reset()
{
    chorus.reset();
    phaser.reset();
    reverb.reset();
    compressor.reset();
    compressorMakeup.reset();
    delayLine.reset();
    delayDampingState = {};
}

void SourceFxChain::setParameter (Param param, float newValue) noexcept
{
    const auto& info = SourceFx::getParamInfo (param);
    parameters[(size_t) param].store (juce::jlimit (info.minValue, info.maxValue, newValue));
}

void SourceFxChain::setReverbParameters (const juce::Reverb::Parameters& params)
{
    const juce::SpinLock::ScopedLockType sl (reverbParametersLock);
    pendingReverbParameters = params;
    reverbParametersChanged = true;
}

void SourceFxChain::updateProcessors()
{
    // Audio thread (or prepare): apply the parameters that changed since the last block
    std::array<bool, SourceFx::numParams> changed {};
    bool anyChanged = firstUpdate;
    for (size_t i = 0; i < parameters.size(); ++i)
    {
        const float newValue = parameters[i].load (std::memory_order_relaxed);
        changed[i] = firstUpdate || newValue != currentValues[i];
        if (changed[i])
        {
            anyChanged = true;
            // Effects that are switched on start from a clean state (no tails from the last time they were used)
            if (SourceFx::getParamInfos()[i].isToggle && newValue >= 0.5f && currentValues[i] < 0.5f && ! firstUpdate)
            {
                switch ((Param) i)
                {
                    case Param::chorusEnabled:     chorus.reset(); break;
                    case Param::phaserEnabled:     phaser.reset(); break;
                    case Param::delayEnabled:      delayLine.reset(); delayDampingState = {}; break;
                    case Param::compressorEnabled: compressor.reset(); break;
                    default: break;
                }
            }
            currentValues[i] = newValue;
        }
    }
    auto hasChanged = [&changed] (Param p) { return changed[(size_t) p]; };

    if (anyChanged)
    {
        if (hasChanged (Param::driveAmount))  driveGain.setTargetValue (juce::Decibels::decibelsToGain (value (Param::driveAmount)));
        if (hasChanged (Param::driveOutput))  driveOutputGain.setTargetValue (juce::Decibels::decibelsToGain (value (Param::driveOutput)));
        if (hasChanged (Param::driveMix))     driveMix.setTargetValue (value (Param::driveMix));

        if (hasChanged (Param::chorusRate))        chorus.setRate (value (Param::chorusRate));
        if (hasChanged (Param::chorusDepth))       chorus.setDepth (value (Param::chorusDepth));
        if (hasChanged (Param::chorusCentreDelay)) chorus.setCentreDelay (value (Param::chorusCentreDelay));
        if (hasChanged (Param::chorusFeedback))    chorus.setFeedback (value (Param::chorusFeedback));
        if (hasChanged (Param::chorusMix))         chorus.setMix (value (Param::chorusMix));

        if (hasChanged (Param::phaserRate))            phaser.setRate (value (Param::phaserRate));
        if (hasChanged (Param::phaserDepth))           phaser.setDepth (value (Param::phaserDepth));
        if (hasChanged (Param::phaserCentreFrequency)) phaser.setCentreFrequency (value (Param::phaserCentreFrequency));
        if (hasChanged (Param::phaserFeedback))        phaser.setFeedback (value (Param::phaserFeedback));
        if (hasChanged (Param::phaserMix))             phaser.setMix (value (Param::phaserMix));

        if (hasChanged (Param::delayTime))
        {
            const float samples = value (Param::delayTime) * 0.001f * (float) sampleRate;
            if (firstUpdate)
                delayTimeSamples.setCurrentAndTargetValue (samples);
            else
                delayTimeSamples.setTargetValue (samples);
        }
        if (hasChanged (Param::delayDamping))
            delayDampingCoefficient = std::exp (-juce::MathConstants<float>::twoPi * juce::jmin (value (Param::delayDamping), (float) sampleRate * 0.45f) / (float) sampleRate);

        if (hasChanged (Param::compressorThreshold)) compressor.setThreshold (value (Param::compressorThreshold));
        if (hasChanged (Param::compressorRatio))     compressor.setRatio (value (Param::compressorRatio));
        if (hasChanged (Param::compressorAttack))    compressor.setAttack (value (Param::compressorAttack));
        if (hasChanged (Param::compressorRelease))   compressor.setRelease (value (Param::compressorRelease));
        if (hasChanged (Param::compressorMakeup))    compressorMakeup.setGainDecibels (value (Param::compressorMakeup));

        // Values don't glide when the chain starts or when an effect is switched on
        if (firstUpdate || (hasChanged (Param::driveEnabled) && isOn (Param::driveEnabled)))
            for (auto* smoothed : { &driveGain, &driveOutputGain, &driveMix })
                smoothed->setCurrentAndTargetValue (smoothed->getTargetValue());
        if (firstUpdate || (hasChanged (Param::delayEnabled) && isOn (Param::delayEnabled)))
            delayTimeSamples.setCurrentAndTargetValue (delayTimeSamples.getTargetValue());
    }
    firstUpdate = false;

    if (reverbParametersChanged.load())
    {
        const juce::SpinLock::ScopedTryLockType sl (reverbParametersLock);
        if (sl.isLocked())
        {
            auto params = pendingReverbParameters;
            // juce::Reverb internally scales the dry level by 2 (and the wet level by 3), so a dry level of 1.0 would
            // boost the signal by +6 dB. Compensate here so that dryLevel = 1.0 means unity gain.
            params.dryLevel *= 0.5f;
            reverb.setParameters (params);
            reverbParametersChanged = false;
        }
    }
}

void SourceFxChain::process (const juce::dsp::ProcessContextReplacing<float>& context)
{
    updateProcessors();
    auto& block = context.getOutputBlock();

    if (isOn (Param::driveEnabled))
        processDrive (block);
    if (isOn (Param::chorusEnabled))
        chorus.process (context);
    if (isOn (Param::phaserEnabled))
        phaser.process (context);
    if (isOn (Param::delayEnabled))
        processDelay (block);
    reverb.process (context);  // Always on (wet level 0 by default)
    if (isOn (Param::compressorEnabled))
    {
        compressor.process (context);
        compressorMakeup.process (context);
    }
}

void SourceFxChain::processDrive (juce::dsp::AudioBlock<float>& block)
{
    // Soft clipping (tanh) with input gain, output gain and dry/wet mix
    const auto numSamples = block.getNumSamples();
    const auto channels = block.getNumChannels();
    for (size_t i = 0; i < numSamples; ++i)
    {
        const float gain = driveGain.getNextValue();
        const float output = driveOutputGain.getNextValue();
        const float mix = driveMix.getNextValue();
        for (size_t ch = 0; ch < channels; ++ch)
        {
            auto* data = block.getChannelPointer (ch);
            const float dry = data[i];
            const float wet = std::tanh (dry * gain) * output;
            data[i] = dry + mix * (wet - dry);
        }
    }
}

void SourceFxChain::processDelay (juce::dsp::AudioBlock<float>& block)
{
    // Feedback delay with a low-pass filter in the feedback path. The wet signal is added to the dry signal.
    // In ping-pong mode the (mono) input enters the left channel and the repeats alternate between channels.
    const auto numSamples = block.getNumSamples();
    const int channels = (int) juce::jmin ((size_t) 2, block.getNumChannels());
    if (channels == 0)
        return;
    const float feedback = value (Param::delayFeedback);
    const float mix = value (Param::delayMix);
    const bool pingPong = isOn (Param::delayPingPong) && channels == 2;
    const float maxDelay = (float) delayLine.getMaximumDelayInSamples() - 2.0f;
    float* data[2] = { block.getChannelPointer (0), channels > 1 ? block.getChannelPointer (1) : nullptr };

    for (size_t i = 0; i < numSamples; ++i)
    {
        const float delaySamples = juce::jlimit (1.0f, maxDelay, delayTimeSamples.getNextValue());
        float delayed[2] = { 0.0f, 0.0f };
        for (int ch = 0; ch < channels; ++ch)
        {
            delayed[ch] = delayLine.popSample (ch, delaySamples, true);
            // One-pole low-pass (damping of the repeats)
            delayDampingState[(size_t) ch] = delayed[ch] + delayDampingCoefficient * (delayDampingState[(size_t) ch] - delayed[ch]);
        }
        const float inputs[2] = { data[0][i], channels > 1 ? data[1][i] : 0.0f };
        for (int ch = 0; ch < channels; ++ch)
        {
            float toDelay;
            if (pingPong)
                toDelay = (ch == 0 ? 0.5f * (inputs[0] + inputs[1]) : 0.0f) + feedback * delayDampingState[(size_t) (1 - ch)];
            else
                toDelay = inputs[ch] + feedback * delayDampingState[(size_t) ch];
            delayLine.pushSample (ch, toDelay);
            data[ch][i] = inputs[ch] + mix * delayed[ch];
        }
    }
}
