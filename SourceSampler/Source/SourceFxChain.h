/*
  ==============================================================================

    SourceFxChain.h

    Effects applied to the output of the sampler, in this order:
    drive -> chorus -> phaser -> delay -> reverb -> compressor.

    The parameters of all effects but reverb are described in a table (see
    getFxParameterInfos), which is used to store them in the preset state, to
    handle the /set_fx_parameter action and to build the UI. Parameters can be
    set from any thread: they are stored in atomics and applied to the
    processors in the audio thread.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>


namespace SourceFx
{
    enum class Param
    {
        driveEnabled, driveAmount, driveOutput, driveMix,
        chorusEnabled, chorusRate, chorusDepth, chorusCentreDelay, chorusFeedback, chorusMix,
        phaserEnabled, phaserRate, phaserDepth, phaserCentreFrequency, phaserFeedback, phaserMix,
        delayEnabled, delayTime, delayFeedback, delayDamping, delayPingPong, delayMix,
        compressorEnabled, compressorThreshold, compressorRatio, compressorAttack, compressorRelease, compressorMakeup,
        numParams
    };
    constexpr int numParams = (int) Param::numParams;

    struct ParamInfo
    {
        const char* id;          // Property name in the preset state (and in the /set_fx_parameter action)
        const char* effect;      // Effect the parameter belongs to
        const char* label;
        float minValue, maxValue, defaultValue, interval;
        const char* unit;
        bool isToggle;           // On/off parameters (0 or 1)
        float skewMidPoint;      // Value at the middle of the slider (0 means linear)

        juce::Identifier getIdentifier() const { return juce::Identifier (id); }
    };

    inline const std::array<ParamInfo, numParams>& getParamInfos()
    {
        static const std::array<ParamInfo, numParams> infos {{
            { "fxDriveEnabled",           "Drive",      "On",            0.0f,    1.0f,    0.0f,   1.0f,   "",   true,  0.0f },
            { "fxDriveAmount",            "Drive",      "Drive",         0.0f,   40.0f,   12.0f,   0.1f,   "dB", false, 0.0f },
            { "fxDriveOutput",            "Drive",      "Output",      -30.0f,    6.0f,   -6.0f,   0.1f,   "dB", false, 0.0f },
            { "fxDriveMix",               "Drive",      "Mix",           0.0f,    1.0f,    1.0f,   0.01f,  "",   false, 0.0f },

            { "fxChorusEnabled",          "Chorus",     "On",            0.0f,    1.0f,    0.0f,   1.0f,   "",   true,  0.0f },
            { "fxChorusRate",             "Chorus",     "Rate",          0.05f,  10.0f,    0.8f,   0.01f,  "Hz", false, 1.0f },
            { "fxChorusDepth",            "Chorus",     "Depth",         0.0f,    1.0f,    0.3f,   0.01f,  "",   false, 0.0f },
            { "fxChorusCentreDelay",      "Chorus",     "Delay",         1.0f,   30.0f,    7.0f,   0.1f,   "ms", false, 0.0f },
            { "fxChorusFeedback",         "Chorus",     "Feedback",     -0.95f,   0.95f,   0.0f,   0.01f,  "",   false, 0.0f },
            { "fxChorusMix",              "Chorus",     "Mix",           0.0f,    1.0f,    0.5f,   0.01f,  "",   false, 0.0f },

            { "fxPhaserEnabled",          "Phaser",     "On",            0.0f,    1.0f,    0.0f,   1.0f,   "",   true,  0.0f },
            { "fxPhaserRate",             "Phaser",     "Rate",          0.05f,  10.0f,    0.5f,   0.01f,  "Hz", false, 1.0f },
            { "fxPhaserDepth",            "Phaser",     "Depth",         0.0f,    1.0f,    0.5f,   0.01f,  "",   false, 0.0f },
            { "fxPhaserCentreFrequency",  "Phaser",     "Centre",       50.0f, 5000.0f, 1000.0f,   1.0f,   "Hz", false, 800.0f },
            { "fxPhaserFeedback",         "Phaser",     "Feedback",     -0.95f,   0.95f,   0.3f,   0.01f,  "",   false, 0.0f },
            { "fxPhaserMix",              "Phaser",     "Mix",           0.0f,    1.0f,    0.5f,   0.01f,  "",   false, 0.0f },

            { "fxDelayEnabled",           "Delay",      "On",            0.0f,    1.0f,    0.0f,   1.0f,   "",   true,  0.0f },
            { "fxDelayTime",              "Delay",      "Time",          1.0f, 2000.0f,  350.0f,   1.0f,   "ms", false, 300.0f },
            { "fxDelayFeedback",          "Delay",      "Feedback",      0.0f,    0.95f,   0.4f,   0.01f,  "",   false, 0.0f },
            { "fxDelayDamping",           "Delay",      "Damping",     500.0f, 20000.0f, 6000.0f, 1.0f,   "Hz", false, 4000.0f },
            { "fxDelayPingPong",          "Delay",      "Ping-pong",     0.0f,    1.0f,    0.0f,   1.0f,   "",   true,  0.0f },
            { "fxDelayMix",               "Delay",      "Mix",           0.0f,    1.0f,    0.35f,  0.01f,  "",   false, 0.0f },

            { "fxCompressorEnabled",      "Compressor", "On",            0.0f,    1.0f,    0.0f,   1.0f,   "",   true,  0.0f },
            { "fxCompressorThreshold",    "Compressor", "Threshold",   -60.0f,    0.0f,  -18.0f,   0.1f,   "dB", false, 0.0f },
            { "fxCompressorRatio",        "Compressor", "Ratio",         1.0f,   20.0f,    4.0f,   0.1f,   ":1", false, 4.0f },
            { "fxCompressorAttack",       "Compressor", "Attack",        0.1f,  200.0f,   10.0f,   0.1f,   "ms", false, 20.0f },
            { "fxCompressorRelease",      "Compressor", "Release",       5.0f, 2000.0f,  150.0f,   1.0f,   "ms", false, 200.0f },
            { "fxCompressorMakeup",       "Compressor", "Makeup",        0.0f,   24.0f,    0.0f,   0.1f,   "dB", false, 0.0f },
        }};
        return infos;
    }

    inline const ParamInfo& getParamInfo (Param p) { return getParamInfos()[(size_t) p]; }

    /** Index of the parameter with the given id, or -1. */
    inline int findParam (const juce::String& id)
    {
        const auto& infos = getParamInfos();
        for (size_t i = 0; i < infos.size(); ++i)
            if (id == infos[i].id)
                return (int) i;
        return -1;
    }

    /** Effects with parameters in the table, in processing order (reverb, processed between delay and compressor,
        has its own parameters). */
    inline juce::StringArray getEffectNames() { return { "Drive", "Chorus", "Phaser", "Delay", "Compressor" }; }
}


class SourceFxChain
{
public:
    SourceFxChain();

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();
    void process (const juce::dsp::ProcessContextReplacing<float>& context);  // Audio thread

    /** Can be called from any thread. Values are clamped to the parameter range. */
    void setParameter (SourceFx::Param param, float value) noexcept;
    float getParameter (SourceFx::Param param) const noexcept { return parameters[(size_t) param].load(); }

    /** Can be called from any thread (applied in the audio thread). dryLevel = 1 is unity gain. */
    void setReverbParameters (const juce::Reverb::Parameters& params);

    static constexpr float maxDelayTimeMs = 2000.0f;

private:
    bool isOn (SourceFx::Param p) const noexcept { return currentValues[(size_t) p] >= 0.5f; }
    float value (SourceFx::Param p) const noexcept { return currentValues[(size_t) p]; }
    void updateProcessors();  // Audio thread: applies changed parameters
    void processDrive (juce::dsp::AudioBlock<float>& block);
    void processDelay (juce::dsp::AudioBlock<float>& block);

    std::array<std::atomic<float>, SourceFx::numParams> parameters;
    std::array<float, SourceFx::numParams> currentValues {};
    bool firstUpdate = true;

    // Reverb parameters are set as a whole from other threads
    juce::SpinLock reverbParametersLock;
    juce::Reverb::Parameters pendingReverbParameters;
    std::atomic<bool> reverbParametersChanged { false };

    double sampleRate = 44100.0;
    int numChannels = 2;

    juce::dsp::Chorus<float> chorus;
    juce::dsp::Phaser<float> phaser;
    juce::dsp::Reverb reverb;
    juce::dsp::Compressor<float> compressor;
    juce::dsp::Gain<float> compressorMakeup;

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLine { 2 };
    juce::SmoothedValue<float> delayTimeSamples;
    std::array<float, 2> delayDampingState {};
    float delayDampingCoefficient = 0.0f;
    juce::SmoothedValue<float> driveGain, driveOutputGain, driveMix;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SourceFxChain)
};
