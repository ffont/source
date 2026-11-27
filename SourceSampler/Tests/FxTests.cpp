/*
  ==============================================================================

    FxTests.cpp - tests for the effects chain (SourceFxChain) and its parameters

  ==============================================================================
*/

#include <JuceHeader.h>
#include "SourceFxChain.h"
#include "SourceSampler.h"
#include "helpers_source.h"
#include "TestUtils.h"

using SourceFx::Param;


class FxChainTests : public juce::UnitTest
{
public:
    FxChainTests() : juce::UnitTest ("Effects chain", "Source") {}

    static constexpr double sampleRate = 48000.0;
    static constexpr int blockSize = 256;

    /** Processes the buffer through the chain in blocks (like the audio thread). */
    static void process (SourceFxChain& fx, juce::AudioBuffer<float>& buffer)
    {
        for (int start = 0; start < buffer.getNumSamples(); start += blockSize)
        {
            const int n = juce::jmin (blockSize, buffer.getNumSamples() - start);
            juce::dsp::AudioBlock<float> block (buffer);
            auto sub = block.getSubBlock ((size_t) start, (size_t) n);
            fx.process (juce::dsp::ProcessContextReplacing<float> (sub));
        }
    }

    static juce::AudioBuffer<float> sine (float frequency, float amplitude, int numSamples)
    {
        juce::AudioBuffer<float> buffer (2, numSamples);
        for (int i = 0; i < numSamples; ++i)
            for (int ch = 0; ch < 2; ++ch)
                buffer.setSample (ch, i, amplitude * std::sin (juce::MathConstants<float>::twoPi * frequency * (float) i / (float) sampleRate));
        return buffer;
    }

    static std::unique_ptr<SourceFxChain> makeChain()
    {
        auto fx = std::make_unique<SourceFxChain>();
        fx->prepare ({ sampleRate, (juce::uint32) blockSize, 2 });
        return fx;
    }

    static bool allFinite (const juce::AudioBuffer<float>& buffer)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                if (! std::isfinite (buffer.getSample (ch, i)))
                    return false;
        return true;
    }

    void runTest() override
    {
        beginTest ("Parameter table is consistent");
        {
            juce::StringArray ids;
            for (const auto& info : SourceFx::getParamInfos())
            {
                expect (info.minValue < info.maxValue, info.id);
                expect (info.defaultValue >= info.minValue && info.defaultValue <= info.maxValue, info.id);
                expect (juce::String (info.id).startsWith ("fx"), info.id);
                expect (SourceFx::getEffectNames().contains (info.effect) || juce::String (info.effect) == "Reverb", info.id);
                expect (! ids.contains (info.id), juce::String ("Duplicated ") + info.id);
                ids.add (info.id);
                if (info.isToggle)
                    expect (info.defaultValue == 0.0f || info.defaultValue == 1.0f);
            }
            for (const auto& effect : SourceFx::getEffectNames())
                expect (SourceFx::findParam ("fx" + effect + "Enabled") >= 0, effect);
            expectEquals (SourceFx::findParam ("nope"), -1);
        }

        beginTest ("With all effects off (and default reverb) the signal is not modified");
        {
            auto fx = makeChain();
            auto input = sine (440.0f, 0.5f, 4800);
            auto output = input;
            process (*fx, output);
            for (int i = 0; i < input.getNumSamples(); ++i)
                expectWithinAbsoluteError (output.getSample (0, i), input.getSample (0, i), 1.0e-3f);
        }

        beginTest ("Delay repeats the signal after the delay time");
        {
            auto fx = makeChain();
            fx->setParameter (Param::delayEnabled, 1.0f);
            fx->setParameter (Param::delayTime, 100.0f);
            fx->setParameter (Param::delayFeedback, 0.0f);
            fx->setParameter (Param::delayMix, 1.0f);
            fx->setParameter (Param::delayDamping, 20000.0f);
            juce::AudioBuffer<float> buffer (2, 12000);
            buffer.clear();
            for (int i = 0; i < 240; ++i)  // 5 ms burst
                for (int ch = 0; ch < 2; ++ch)
                    buffer.setSample (ch, i, 0.5f);
            process (*fx, buffer);
            const int delaySamples = (int) (0.1 * sampleRate);
            expectGreaterThan (buffer.getMagnitude (0, delaySamples + 20, 200), 0.3f, "No repeat after the delay time");
            expectLessThan (buffer.getMagnitude (0, 400, delaySamples - 500), 1.0e-4f, "Signal before the repeat");
            expectLessThan (buffer.getMagnitude (0, 2 * delaySamples + 300, 1000), 1.0e-3f, "Repeats with no feedback");
            expect (allFinite (buffer));
        }

        beginTest ("Delay feedback produces decaying repeats, ping-pong alternates channels");
        {
            auto fx = makeChain();
            fx->setParameter (Param::delayEnabled, 1.0f);
            fx->setParameter (Param::delayTime, 50.0f);
            fx->setParameter (Param::delayFeedback, 0.5f);
            fx->setParameter (Param::delayMix, 1.0f);
            fx->setParameter (Param::delayDamping, 20000.0f);
            fx->setParameter (Param::delayPingPong, 1.0f);
            juce::AudioBuffer<float> buffer (2, 9600);
            buffer.clear();
            for (int i = 0; i < 100; ++i)
                for (int ch = 0; ch < 2; ++ch)
                    buffer.setSample (ch, i, 0.5f);
            process (*fx, buffer);
            const int d = (int) (0.05 * sampleRate);
            const float firstLeft = buffer.getMagnitude (0, d + 10, 80), firstRight = buffer.getMagnitude (1, d + 10, 80);
            const float secondLeft = buffer.getMagnitude (0, 2 * d + 10, 80), secondRight = buffer.getMagnitude (1, 2 * d + 10, 80);
            expectGreaterThan (firstLeft, 0.2f);
            expectLessThan (firstRight, 0.01f, "First repeat only on the left");
            expectGreaterThan (secondRight, 0.05f, "Second repeat on the right");
            expectLessThan (secondLeft, 0.01f);
            expectLessThan (secondRight, firstLeft, "Repeats decay");
        }

        beginTest ("Drive is bounded by its output level");
        {
            auto fx = makeChain();
            fx->setParameter (Param::driveEnabled, 1.0f);
            fx->setParameter (Param::driveAmount, 40.0f);
            fx->setParameter (Param::driveOutput, -6.0f);
            fx->setParameter (Param::driveMix, 1.0f);
            auto buffer = sine (220.0f, 0.9f, 9600);
            process (*fx, buffer);
            expectLessThan (buffer.getMagnitude (0, 2400, 7200), juce::Decibels::decibelsToGain (-6.0f) + 0.01f);
            expectGreaterThan (buffer.getRMSLevel (0, 2400, 7200), 0.4f, "Heavily driven sine is close to a square wave");
        }

        beginTest ("Compressor reduces the level of loud signals");
        {
            auto loud = sine (440.0f, 0.9f, 48000);
            auto compressed = loud;
            auto fx = makeChain();
            fx->setParameter (Param::compressorEnabled, 1.0f);
            fx->setParameter (Param::compressorThreshold, -30.0f);
            fx->setParameter (Param::compressorRatio, 10.0f);
            fx->setParameter (Param::compressorMakeup, 0.0f);
            process (*fx, compressed);
            expectLessThan (compressed.getRMSLevel (0, 24000, 24000), loud.getRMSLevel (0, 24000, 24000) * 0.3f);
        }

        beginTest ("Chorus and phaser modify the signal without blowing up");
        {
            for (auto enabled : { Param::chorusEnabled, Param::phaserEnabled })
            {
                auto fx = makeChain();
                fx->setParameter (enabled, 1.0f);
                fx->setParameter (Param::chorusMix, 1.0f);
                fx->setParameter (Param::phaserMix, 1.0f);
                fx->setParameter (Param::chorusFeedback, 0.95f);
                fx->setParameter (Param::phaserFeedback, 0.95f);
                auto input = sine (440.0f, 0.5f, 48000);
                auto output = input;
                process (*fx, output);
                expect (allFinite (output));
                float maxDifference = 0.0f;
                for (int i = 0; i < input.getNumSamples(); ++i)
                    maxDifference = juce::jmax (maxDifference, std::abs (output.getSample (0, i) - input.getSample (0, i)));
                expectGreaterThan (maxDifference, 0.05f);
                expectLessThan (output.getMagnitude (0, 0, 48000), 10.0f);
            }
        }

        beginTest ("Reverb can be switched off");
        {
            auto fx = makeChain();
            juce::Reverb::Parameters reverb;
            reverb.wetLevel = 1.0f;
            reverb.dryLevel = 0.0f;
            fx->setReverbParameters (reverb);
            auto wet = sine (440.0f, 0.5f, 4800);
            process (*fx, wet);
            expectLessThan (std::abs (wet.getSample (0, 4000) - 0.5f * std::sin (juce::MathConstants<float>::twoPi * 440.0f * 4000.0f / (float) sampleRate)), 0.9f);
            fx->setParameter (Param::reverbEnabled, 0.0f);
            auto input = sine (440.0f, 0.5f, 4800);
            auto output = input;
            process (*fx, output);
            for (int i = 0; i < input.getNumSamples(); i += 37)
                expectWithinAbsoluteError (output.getSample (0, i), input.getSample (0, i), 1.0e-6f);
        }

        beginTest ("Parameters are clamped");
        {
            auto fx = makeChain();
            fx->setParameter (Param::delayTime, 100000.0f);
            expectEquals (fx->getParameter (Param::delayTime), SourceFxChain::maxDelayTimeMs);
            fx->setParameter (Param::delayFeedback, 5.0f);
            expectLessThan (fx->getParameter (Param::delayFeedback), 1.0f);
        }
    }
};


class FxStateTests : public juce::UnitTest
{
public:
    FxStateTests() : juce::UnitTest ("Effects state", "Source") {}

    void runTest() override
    {
        beginTest ("Effects parameters are set with actions, stored in presets and default to off");
        {
            TestUtils::TemporaryFolder tmp;
            SourceSampler sampler (tmp.folder);
            sampler.prepareToPlay (48000.0, 256);
            auto send = [&] (const juce::String& action, const juce::StringArray& parameters)
            {
                sampler.actionListenerCallback (SourceHelpers::serializeAction (action, parameters));
            };
            auto preset = [&] { return sampler.state.getChildWithName (SourceIDs::PRESET); };

            for (const auto& info : SourceFx::getParamInfos())
                expectEquals ((float) preset().getProperty (info.getIdentifier()), info.defaultValue, info.id);
            expect (! (bool) preset().getProperty ("fxDelayEnabled"), "Effects are off by default");

            send (ACTION_SET_FX_PARAMETER, { "fxDelayEnabled", "1" });
            send (ACTION_SET_FX_PARAMETER, { "fxDelayTime", "250" });
            send (ACTION_SET_FX_PARAMETER, { "fxDelayFeedback", "7" });  // Clamped
            send (ACTION_SET_FX_PARAMETER, { "notAParameter", "1" });
            expect ((bool) preset().getProperty ("fxDelayEnabled"));
            expectEquals ((float) preset().getProperty ("fxDelayTime"), 250.0f);
            expectEquals ((float) preset().getProperty ("fxDelayFeedback"), 0.95f);
            expect (! preset().hasProperty ("notAParameter"));

            send (ACTION_SAVE_PRESET, { "FX preset", "3" });
            send (ACTION_LOAD_PRESET, { "4" });  // Empty preset: defaults
            expect (! (bool) preset().getProperty ("fxDelayEnabled"));
            send (ACTION_LOAD_PRESET, { "3" });
            expect ((bool) preset().getProperty ("fxDelayEnabled"));
            expectEquals ((float) preset().getProperty ("fxDelayTime"), 250.0f);
        }
    }
};


static FxChainTests fxChainTests;
static FxStateTests fxStateTests;
