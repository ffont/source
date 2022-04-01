/*
  ==============================================================================

    helpers.h
    Created: 22 Mar 2022 10:12:20pm
    Author:  Frederic Font Corbera

  ==============================================================================
*/



#pragma once

#include <JuceHeader.h>
#include "defines.h"
#include "drow_ValueTreeObjectList.h"


struct GlobalContextStruct {
    double sampleRate = 0.0;
    int samplesPerBlock = 0;
    Synthesiser* sampler = nullptr;
    File sourceDataLocation;
    File soundsDownloadLocation;
    File presetFilesLocation;
    File tmpFilesLocation;
};


namespace Helpers
{

    inline juce::ValueTree createUuidProperty (juce::ValueTree& v)
    {
        if (! v.hasProperty (IDs::uuid))
            v.setProperty (IDs::uuid, juce::Uuid().toString(), nullptr);
        return v;
    }

    inline juce::ValueTree createEmptyState()
    {
        juce::ValueTree state (IDs::SOURCE_STATE);
        Helpers::createUuidProperty (state);
        state.setProperty (IDs::currentPresetIndex, Defaults::currentPresetIndex, nullptr);
        return state;
    }

    inline juce::ValueTree createEmptyPresetState()
    {
        juce::ValueTree preset (IDs::PRESET);
        Helpers::createUuidProperty (preset);
        preset.setProperty (IDs::name, juce::Time::getCurrentTime().formatted("%Y%m%d") + " unnamed", nullptr);
        preset.setProperty (IDs::noteLayoutType, Defaults::noteLayoutType, nullptr);
        return preset;
    }

    inline juce::ValueTree createEmptySourceSoundState(const juce::String soundName)
    {
        juce::ValueTree sound (IDs::SOUND);
        Helpers::createUuidProperty (sound);
        sound.setProperty (IDs::name, soundName, nullptr);
        sound.setProperty (IDs::enabled, true, nullptr);
        // --> Start auto-generated code A
        sound.setProperty (IDs::soundType, 0, nullptr);
        sound.setProperty (IDs::launchMode, 0, nullptr);
        sound.setProperty (IDs::startPosition, 0.0f, nullptr);
        sound.setProperty (IDs::endPosition, 1.0f, nullptr);
        sound.setProperty (IDs::loopStartPosition, 0.0f, nullptr);
        sound.setProperty (IDs::loopEndPosition, 1.0f, nullptr);
        sound.setProperty (IDs::loopXFadeNSamples, 500, nullptr);
        sound.setProperty (IDs::reverse, 0, nullptr);
        sound.setProperty (IDs::noteMappingMode, 0, nullptr);
        sound.setProperty (IDs::numSlices, 0, nullptr);
        sound.setProperty (IDs::playheadPosition, 0.0f, nullptr);
        sound.setProperty (IDs::freezePlayheadSpeed, 100.0f, nullptr);
        sound.setProperty (IDs::filterCutoff, 20000.0f, nullptr);
        sound.setProperty (IDs::filterRessonance, 0.0f, nullptr);
        sound.setProperty (IDs::filterKeyboardTracking, 0.0f, nullptr);
        sound.setProperty (IDs::filterAttack, 0.01f, nullptr);
        sound.setProperty (IDs::filterDecay, 0.0f, nullptr);
        sound.setProperty (IDs::filterSustain, 1.0f, nullptr);
        sound.setProperty (IDs::filterRelease, 0.01f, nullptr);
        sound.setProperty (IDs::filterADSR2CutoffAmt, 1.0f, nullptr);
        sound.setProperty (IDs::gain, -10.0f, nullptr);
        sound.setProperty (IDs::attack, 0.01f, nullptr);
        sound.setProperty (IDs::decay, 0.0f, nullptr);
        sound.setProperty (IDs::sustain, 1.0f, nullptr);
        sound.setProperty (IDs::release, 0.01f, nullptr);
        sound.setProperty (IDs::pan, 0.0f, nullptr);
        sound.setProperty (IDs::pitch, 0.0f, nullptr);
        sound.setProperty (IDs::pitchBendRangeUp, 12.0f, nullptr);
        sound.setProperty (IDs::pitchBendRangeDown, 12.0f, nullptr);
        sound.setProperty (IDs::mod2CutoffAmt, 10.0f, nullptr);
        sound.setProperty (IDs::mod2GainAmt, 6.0f, nullptr);
        sound.setProperty (IDs::mod2PitchAmt, 0.0f, nullptr);
        sound.setProperty (IDs::mod2PlayheadPos, 0.0f, nullptr);
        sound.setProperty (IDs::vel2CutoffAmt, 0.0f, nullptr);
        sound.setProperty (IDs::vel2GainAmt, 0.5f, nullptr);
        // --> End auto-generated code A
        return sound;
    }

    inline juce::ValueTree createSourceSampleSoundState(const juce::String soundName, int soundId, const juce::String previewURL)
    {
        juce::ValueTree ss (IDs::SOUND_SAMPLE);
        Helpers::createUuidProperty (ss);
        ss.setProperty (IDs::name, soundName, nullptr);
        ss.setProperty (IDs::soundId, soundId, nullptr);
        ss.setProperty (IDs::previewURL, previewURL, nullptr);
        ss.setProperty (IDs::midiRootNote, 64, nullptr);
        ss.setProperty (IDs::duration, -1.0, nullptr);
        BigInteger midiNotes;
        midiNotes.setRange(0, 127, true);
        ss.setProperty (IDs::midiNotes, midiNotes.toString(16), nullptr);
        return ss;
    }

    inline juce::ValueTree createAnalysisFromFreesoundAnalysis(juce::var fsAnalysisValueTree)
    {
        juce::ValueTree soundAnalysis = juce::ValueTree(IDs::ANALYSIS);
        if (fsAnalysisValueTree.hasProperty("rhythm")){
            if (fsAnalysisValueTree["rhythm"].hasProperty("onset_times")){
                juce::ValueTree soundAnalysisOnsetTimes = juce::ValueTree(IDs::onsets);
                for (int j=0; j<fsAnalysisValueTree["rhythm"]["onset_times"].size(); j++){
                    ValueTree onset = ValueTree(IDs::onset);
                    onset.setProperty(IDs::onsetTime, fsAnalysisValueTree["rhythm"]["onset_times"][j], nullptr);
                    soundAnalysisOnsetTimes.appendChild(onset, nullptr);
                }
                soundAnalysis.appendChild(soundAnalysisOnsetTimes, nullptr);
            }
        }
        return soundAnalysis;
    }

    inline juce::ValueTree createMidiMappingState(int ccNumber, String parameterName, float minRange, float maxRange)
    {
        juce::ValueTree mapping (IDs::MIDI_CC_MAPPING);
        Helpers::createUuidProperty (mapping);
        mapping.setProperty (IDs::ccNumber, ccNumber, nullptr);
        mapping.setProperty (IDs::parameterName, parameterName, nullptr);
        mapping.setProperty (IDs::minRange, minRange, nullptr);
        mapping.setProperty (IDs::maxRange, maxRange, nullptr);
        return mapping;
    }

    inline juce::ValueTree createDefaultState(int numSounds)
    {
        juce::ValueTree state = createEmptyState();
        juce::ValueTree preset = createEmptyPresetState();
    
        for (int sn = 0; sn < numSounds; ++sn)
        {
            juce::ValueTree s = createEmptySourceSoundState("Sound " + juce::String (sn + 1));
            juce::ValueTree ss = createSourceSampleSoundState("433328 - 1", 433328, "https://freesound.org/data/previews/433/433328_735175-hq.ogg");
            s.addChild(ss, -1, nullptr);
            preset.addChild (s, -1, nullptr);
        }
        state.addChild (preset, -1, nullptr);

        return state;
    }
}


class MidiCCMapping
{
public:
    
    MidiCCMapping(const juce::ValueTree& _state): state(_state)
    {
        bindState();
    }
    
    juce::ValueTree state;
    void bindState ()
    {
        ccNumber.referTo(state, IDs::ccNumber, nullptr, Defaults::ccNumber);
        minRange.referTo(state, IDs::minRange, nullptr, Defaults::minRange);
        maxRange.referTo(state, IDs::maxRange, nullptr, Defaults::maxRange);
        parameterName.referTo(state, IDs::parameterName, nullptr);
    }
    
    bool operator< (const MidiCCMapping &other) const {
        if (ccNumber.get() != other.ccNumber.get()){
            return ccNumber.get() < other.ccNumber.get();
        } else {
            return parameterName.get() < other.parameterName.get();
        }
    }

    juce::CachedValue<int> ccNumber;
    juce::CachedValue<juce::String> parameterName;
    juce::CachedValue<float> minRange;
    juce::CachedValue<float> maxRange;    
};


struct MidiCCMappingList: public drow::ValueTreeObjectList<MidiCCMapping>
{
    MidiCCMappingList (const juce::ValueTree& v): drow::ValueTreeObjectList<MidiCCMapping> (v)
    {
        rebuildObjects();
    }

    ~MidiCCMappingList()
    {
        freeObjects();
    }

    bool isSuitableType (const juce::ValueTree& v) const override
    {
        return v.hasType (IDs::MIDI_CC_MAPPING);
    }

    MidiCCMapping* createNewObject (const juce::ValueTree& v) override
    {
        return new MidiCCMapping (v);
    }

    void deleteObject (MidiCCMapping* s) override
    {
        delete s;
    }

    void newObjectAdded (MidiCCMapping* s) override    {}
    void objectRemoved (MidiCCMapping*) override     {}
    void objectOrderChanged() override       {}
    
    void removeMidiCCMappingWithUUID(juce::String uuid)
    {
        parent.removeChild(parent.getChildWithProperty(IDs::uuid, uuid), nullptr);
    }
};
