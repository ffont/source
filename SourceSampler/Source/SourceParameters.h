/*
  ==============================================================================

    SourceParameters.h

    Metadata (range, default value, description) of the per-sound parameters.
    The table is generated from data_for_code_gen.csv with generate_code.py
    (run "python generate_code.py -i" after editing the CSV).

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>


struct SoundParameterInfo
{
    const char* name;
    bool isInt;
    float minValue;
    float maxValue;
    float defaultValue;
    bool midiCCEnabled;
    const char* description;

    juce::Identifier getIdentifier() const { return juce::Identifier (name); }
};


inline const std::vector<SoundParameterInfo>& getSoundParametersInfo()
{
    static const std::vector<SoundParameterInfo> parameters {
    // --> Start auto-generated code A
    { "launchMode", true, 0.0f, 4.0f, 0.0f, false, "gate,loop,loop ping-pong,trigger,freeze" },
    { "startPosition", false, 0.0f, 1.0f, 0.0f, true, "percentage position" },
    { "endPosition", false, 0.0f, 1.0f, 1.0f, true, "percentage position" },
    { "loopStartPosition", false, 0.0f, 1.0f, 0.0f, true, "percentage position" },
    { "loopEndPosition", false, 0.0f, 1.0f, 1.0f, true, "percentage position" },
    { "loopXFadeNSamples", true, 10.0f, 100000.0f, 500.0f, false, "n samples" },
    { "reverse", true, 0.0f, 1.0f, 0.0f, false, "1=reverse" },
    { "noteMappingMode", true, 0.0f, 3.0f, 0.0f, false, "pitch/slice/both/repeat (repeat means sounds is not altered at all)" },
    { "numSlices", true, 0.0f, 100.0f, 0.0f, false, "0=auto per n notes, 1=auto per onsets, 2-100=from 2 to 100 equally spaced slices" },
    { "playheadPosition", false, 0.0f, 1.0f, 0.0f, true, "this is only used in freeze mode, and represents the playhead position from 0=sound start, to 1=sound end" },
    { "freezePlayheadSpeed", false, 1.0f, 5000.0f, 100.0f, true, "speed modifier for the max speed at which playhead moves into the direction of target playhead in freeze mode" },
    { "filterCutoff", false, 10.0f, 20000.0f, 20000.0f, true, "" },
    { "filterRessonance", false, 0.0f, 1.0f, 0.0f, true, "" },
    { "filterKeyboardTracking", false, 0.0f, 1.0f, 0.0f, false, "" },
    { "filterAttack", false, 0.0f, 1.0f, 0.01f, false, "seconds" },
    { "filterDecay", false, 0.0f, 1.0f, 0.0f, false, "seconds" },
    { "filterSustain", false, 0.0f, 1.0f, 1.0f, false, "amount" },
    { "filterRelease", false, 0.0f, 1.0f, 0.01f, false, "seconds" },
    { "filterADSR2CutoffAmt", false, 0.0f, 100.0f, 1.0f, false, "" },
    { "gain", false, -80.0f, 12.0f, -10.0f, true, "db" },
    { "attack", false, 0.0f, 1.0f, 0.01f, false, "seconds" },
    { "decay", false, 0.0f, 1.0f, 0.0f, false, "seconds" },
    { "sustain", false, 0.0f, 1.0f, 1.0f, false, "amount" },
    { "release", false, 0.0f, 1.0f, 0.01f, false, "seconds" },
    { "pan", false, -1.0f, 1.0f, 0.0f, true, "" },
    { "pitch", false, -36.0f, 36.0f, 0.0f, true, "semitones (with respect to midi root note)" },
    { "pitchBendRangeUp", false, 0.0f, 36.0f, 12.0f, false, "semitones" },
    { "pitchBendRangeDown", false, 0.0f, 36.0f, 12.0f, false, "semitones" },
    { "mod2CutoffAmt", false, 0.0f, 100.0f, 10.0f, false, "" },
    { "mod2GainAmt", false, -12.0f, 12.0f, 6.0f, false, "gain amount to be added with AT (in dB)" },
    { "mod2PitchAmt", false, -12.0f, 12.0f, 0.0f, false, "semitones" },
    { "mod2PlayheadPos", false, 0.0f, 1.0f, 0.0f, false, "" },
    { "vel2CutoffAmt", false, 0.0f, 100.0f, 0.0f, false, "velocity to cutoff amount (multiplier to cutoff frequency)" },
    { "vel2GainAmt", false, 0.0f, 1.0f, 0.5f, false, "percentage of how much velocity affects gain" },
    { "velSensitivity", false, 0.0f, 6.0f, 1.0f, false, "modifier to velocity values applies as X^velSensitivity" },
    { "midiChannel", true, 0.0f, 16.0f, 0.0f, false, "global,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16" },
    { "pitchShift", false, -36.0f, 36.0f, 0.0f, false, "semitones (with respect to midi root note)" },
    { "timeStretch", false, 0.1f, 4.0f, 1.0f, false, "time stretch ratio (1=no change, 2=2x slower)" },
    // --> End auto-generated code A
    };
    return parameters;
}

inline const SoundParameterInfo* findSoundParameterInfo (const juce::String& name)
{
    for (const auto& info : getSoundParametersInfo())
        if (name == info.name)
            return &info;
    return nullptr;
}
