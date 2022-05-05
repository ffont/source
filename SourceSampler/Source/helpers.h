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
    int midiInChannel = 0;
    juce::Synthesiser* sampler = nullptr;
    juce::String useOriginalFilesPreference = USE_ORIGINAL_FILES_NEVER;
    juce::File sourceDataLocation;
    juce::File soundsDownloadLocation;
    juce::File presetFilesLocation;
    juce::File tmpFilesLocation;
    juce::String freesoundOauthAccessToken = "";
};


namespace Helpers
{

    inline juce::ValueTree createUuidProperty (juce::ValueTree& v)
    {
        if (! v.hasProperty (IDs::uuid))
            v.setProperty (IDs::uuid, juce::Uuid().toString(), nullptr);
        return v;
    }

    inline void addPropertyWithDefaultValueIfNotExisting(juce::ValueTree& v, juce::Identifier property, bool defaultValue)
    {
        if (!v.hasProperty(property)){
            v.setProperty(property, defaultValue, nullptr);
        }
    }

    inline void addPropertyWithDefaultValueIfNotExisting(juce::ValueTree& v, juce::Identifier property, juce::String defaultValue)
    {
        if (!v.hasProperty(property)){
            v.setProperty(property, defaultValue, nullptr);
        }
    }

    inline void addPropertyWithDefaultValueIfNotExisting(juce::ValueTree& v, juce::Identifier property, int defaultValue)
    {
        if (!v.hasProperty(property)){
            v.setProperty(property, defaultValue, nullptr);
        }
    }

    inline void addPropertyWithDefaultValueIfNotExisting(juce::ValueTree& v, juce::Identifier property, float defaultValue)
    {
        if (!v.hasProperty(property)){
            v.setProperty(property, defaultValue, nullptr);
        }
    }

    inline juce::ValueTree createNewEmptyState()
    {
        juce::ValueTree state (IDs::SOURCE_STATE);
        Helpers::createUuidProperty (state);
        state.setProperty (IDs::currentPresetIndex, Defaults::currentPresetIndex, nullptr);
        state.setProperty (IDs::globalMidiInChannel, Defaults::globalMidiInChannel, nullptr);
        state.setProperty (IDs::midiOutForwardsMidiIn, Defaults::midiOutForwardsMidiIn, nullptr);
        state.setProperty (IDs::useOriginalFilesPreference, Defaults::useOriginalFilesPreference, nullptr);
        return state;
    }

    inline juce::ValueTree createNewStateFromCurrentSatate(juce::ValueTree currentState)
    {
        juce::ValueTree state (IDs::SOURCE_STATE);
        Helpers::createUuidProperty (state);
        state.setProperty (IDs::currentPresetIndex, currentState.getProperty(IDs::currentPresetIndex), nullptr);
        state.setProperty (IDs::globalMidiInChannel, currentState.getProperty(IDs::globalMidiInChannel), nullptr);
        state.setProperty (IDs::midiOutForwardsMidiIn, currentState.getProperty(IDs::midiOutForwardsMidiIn), nullptr);
        state.setProperty (IDs::useOriginalFilesPreference, currentState.getProperty(IDs::useOriginalFilesPreference), nullptr);
        return state;
    }

    inline juce::String defaultPresetName(){
        return juce::Time::getCurrentTime().formatted("%Y%m%d") + " unnamed";
    }

    inline juce::ValueTree createEmptyPresetState()
    {
        juce::ValueTree preset (IDs::PRESET);
        Helpers::createUuidProperty (preset);
        preset.setProperty (IDs::name, defaultPresetName(), nullptr);
        preset.setProperty (IDs::noteLayoutType, Defaults::noteLayoutType, nullptr);
        preset.setProperty (IDs::numVoices, Defaults::numVoices, nullptr);
        preset.setProperty (IDs::reverbDamping, Defaults::reverbDamping, nullptr);
        preset.setProperty (IDs::reverbWetLevel, Defaults::reverbWetLevel, nullptr);
        preset.setProperty (IDs::reverbDryLevel, Defaults::reverbDryLevel, nullptr);
        preset.setProperty (IDs::reverbWidth, Defaults::reverbWidth, nullptr);
        preset.setProperty (IDs::reverbFreezeMode, Defaults::reverbFreezeMode, nullptr);
        preset.setProperty (IDs::reverbRoomSize, Defaults::reverbRoomSize, nullptr);
        return preset;
    }

    inline juce::ValueTree createSourceSampleSoundState(const juce::String soundName, int soundID, const juce::String previewURL, const juce::String format, int sizeBytes, const juce::String filePath)
    {
        juce::ValueTree ss (IDs::SOUND_SAMPLE);
        Helpers::createUuidProperty (ss);
        ss.setProperty (IDs::name, soundName, nullptr);
        ss.setProperty (IDs::downloadProgress, 0, nullptr);
        ss.setProperty (IDs::downloadCompleted, false, nullptr);
        ss.setProperty (IDs::soundId, soundID, nullptr); // This might be -1 if sound is not from freesound
        ss.setProperty (IDs::format, format, nullptr);
        ss.setProperty (IDs::previewURL, previewURL, nullptr); // This might be "" if sound is not from freesound
        ss.setProperty (IDs::filePath, filePath, nullptr);
        ss.setProperty (IDs::duration, -1.0, nullptr);  // This will be set when sound is loaded
        ss.setProperty (IDs::filesize, sizeBytes, nullptr);  // This might be -1 if sound is not from freesound
        ss.setProperty (IDs::soundFromFreesound, soundID > -1, nullptr);
        ss.setProperty (IDs::usesPreview, false, nullptr);
        ss.setProperty (IDs::midiRootNote, Defaults::midiRootNote, nullptr);
        ss.setProperty (IDs::midiVelocityLayer, Defaults::midiVelocityLayer, nullptr);
        return ss;
    }

    inline juce::ValueTree createAnalysisFromSlices(juce::StringArray slices)
    {
        juce::ValueTree soundAnalysis = juce::ValueTree(IDs::ANALYSIS);
        juce::ValueTree soundAnalysisOnsetTimes = juce::ValueTree(IDs::onsets);
        for (auto sliceString: slices){
            juce::ValueTree onset = juce::ValueTree(IDs::onset);
            onset.setProperty(IDs::onsetTime, sliceString.getFloatValue(), nullptr);
            soundAnalysisOnsetTimes.appendChild(onset, nullptr);
        }
        soundAnalysis.appendChild(soundAnalysisOnsetTimes, nullptr);
        return soundAnalysis;
    }

    inline juce::ValueTree createMidiMappingState(int ccNumber, juce::String parameterName, float minRange, float maxRange)
    {
        juce::ValueTree mapping (IDs::MIDI_CC_MAPPING);
        Helpers::createUuidProperty (mapping);
        mapping.setProperty (IDs::ccNumber, ccNumber, nullptr);
        mapping.setProperty (IDs::parameterName, parameterName, nullptr);
        mapping.setProperty (IDs::minRange, minRange, nullptr);
        mapping.setProperty (IDs::maxRange, maxRange, nullptr);
        return mapping;
    }

    inline juce::ValueTree createEmptySourceSoundState()
    {
        juce::ValueTree sound (IDs::SOUND);
        Helpers::createUuidProperty (sound);
        sound.setProperty (IDs::willBeDeleted, false, nullptr);
        sound.setProperty (IDs::allSoundsLoaded, false, nullptr);
        // --> Start auto-generated code A
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
        sound.setProperty (IDs::midiChannel, 0, nullptr);
        // --> End auto-generated code A
        return sound;
    }

    inline juce::ValueTree createSourceSoundAndSourceSamplerSoundFromProperties(int soundID,
                                                                                const juce::String& soundName,
                                                                                const juce::String& soundUser,
                                                                                const juce::String& soundLicense,
                                                                                const juce::String& previewURL,
                                                                                const juce::String& localFilePath,
                                                                                const juce::String& format,
                                                                                int sizeBytes,
                                                                                juce::StringArray slices,
                                                                                juce::BigInteger midiNotes,
                                                                                int midiRootNote,
                                                                                int midiVelocityLayer){
        juce::ValueTree sourceSound = Helpers::createEmptySourceSoundState();
        juce::ValueTree sourceSamplerSound = Helpers::createSourceSampleSoundState(soundName,
                                                                                   soundID,
                                                                                   previewURL,
                                                                                   format,
                                                                                   sizeBytes,
                                                                                   localFilePath);
        sourceSamplerSound.setProperty(IDs::username, soundUser, nullptr);
        sourceSamplerSound.setProperty(IDs::license, soundLicense, nullptr);
        sourceSamplerSound.setProperty(IDs::midiVelocityLayer, midiVelocityLayer, nullptr);

        if (midiRootNote >  -1){
            sourceSamplerSound.setProperty(IDs::midiRootNote, midiRootNote, nullptr);
        }
        if (!midiNotes.isZero()){
            // Note that MIDI notes are stored in Sound (not SourceSamplerSound)
            sourceSound.setProperty(IDs::midiNotes, midiNotes.toString(16), nullptr);
        }
        if (slices.size() > 0){
            juce::ValueTree soundAnalysis = Helpers::createAnalysisFromSlices(slices);
            sourceSamplerSound.addChild(soundAnalysis, -1, nullptr);
        }
        
        sourceSound.addChild(sourceSamplerSound, -1, nullptr);
        
        return sourceSound;
    }

    inline juce::ValueTree createDefaultEmptyState()
    {
        juce::ValueTree state = createNewEmptyState();
        juce::ValueTree preset = createEmptyPresetState();
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
        Helpers::addPropertyWithDefaultValueIfNotExisting(state, IDs::ccNumber, Defaults::ccNumber);
        ccNumber.referTo(state, IDs::ccNumber, nullptr, Defaults::ccNumber);
        Helpers::addPropertyWithDefaultValueIfNotExisting(state, IDs::minRange, Defaults::minRange);
        minRange.referTo(state, IDs::minRange, nullptr, Defaults::minRange);
        Helpers::addPropertyWithDefaultValueIfNotExisting(state, IDs::maxRange, Defaults::maxRange);
        maxRange.referTo(state, IDs::maxRange, nullptr, Defaults::maxRange);
        Helpers::addPropertyWithDefaultValueIfNotExisting(state, IDs::parameterName, Defaults::parameterName);
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
