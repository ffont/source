/*
  ==============================================================================

    helpers.h
    Created: 22 Mar 2022 10:12:20pm
    Author:  Frederic Font Corbera

  ==============================================================================
*/



#pragma once

#include <JuceHeader.h>
#include "defines_source.h"
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
    juce::String freesoundOauthAccessToken = SourceDefaults::freesoundOauthAccessToken;
};


namespace SourceHelpers
{

    inline juce::ValueTree createUuidProperty (juce::ValueTree& v)
    {
        if (! v.hasProperty (SourceIDs::uuid))
            v.setProperty (SourceIDs::uuid, juce::Uuid().toString(), nullptr);
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
        juce::ValueTree state (SourceIDs::SOURCE_STATE);
        SourceHelpers::createUuidProperty (state);
        state.setProperty (SourceIDs::currentPresetIndex, SourceDefaults::currentPresetIndex, nullptr);
        state.setProperty (SourceIDs::globalMidiInChannel, SourceDefaults::globalMidiInChannel, nullptr);
        state.setProperty (SourceIDs::midiOutForwardsMidiIn, SourceDefaults::midiOutForwardsMidiIn, nullptr);
        state.setProperty (SourceIDs::useOriginalFilesPreference, SourceDefaults::useOriginalFilesPreference, nullptr);
        state.setProperty (SourceIDs::freesoundOauthAccessToken, SourceDefaults::freesoundOauthAccessToken, nullptr);
        return state;
    }

    inline juce::ValueTree createNewStateFromCurrentSatate(juce::ValueTree currentState)
    {
        juce::ValueTree state (SourceIDs::SOURCE_STATE);
        state.setProperty (SourceIDs::uuid, currentState.getProperty(SourceIDs::uuid), nullptr); // Keep the main level uuid
        state.setProperty (SourceIDs::currentPresetIndex, currentState.getProperty(SourceIDs::currentPresetIndex), nullptr);
        state.setProperty (SourceIDs::globalMidiInChannel, currentState.getProperty(SourceIDs::globalMidiInChannel), nullptr);
        state.setProperty (SourceIDs::midiOutForwardsMidiIn, currentState.getProperty(SourceIDs::midiOutForwardsMidiIn), nullptr);
        state.setProperty (SourceIDs::useOriginalFilesPreference, currentState.getProperty(SourceIDs::useOriginalFilesPreference), nullptr);
        state.setProperty (SourceIDs::freesoundOauthAccessToken, currentState.getProperty(SourceIDs::freesoundOauthAccessToken), nullptr);
        return state;
    }

    inline juce::String defaultPresetName(){
        return juce::Time::getCurrentTime().formatted("%Y%m%d") + " unnamed";
    }

    inline juce::ValueTree createEmptyPresetState()
    {
        juce::ValueTree preset (SourceIDs::PRESET);
        SourceHelpers::createUuidProperty (preset);
        preset.setProperty (SourceIDs::name, defaultPresetName(), nullptr);
        preset.setProperty (SourceIDs::noteLayoutType, SourceDefaults::noteLayoutType, nullptr);
        preset.setProperty (SourceIDs::numVoices, SourceDefaults::numVoices, nullptr);
        preset.setProperty (SourceIDs::reverbDamping, SourceDefaults::reverbDamping, nullptr);
        preset.setProperty (SourceIDs::reverbWetLevel, SourceDefaults::reverbWetLevel, nullptr);
        preset.setProperty (SourceIDs::reverbDryLevel, SourceDefaults::reverbDryLevel, nullptr);
        preset.setProperty (SourceIDs::reverbWidth, SourceDefaults::reverbWidth, nullptr);
        preset.setProperty (SourceIDs::reverbFreezeMode, SourceDefaults::reverbFreezeMode, nullptr);
        preset.setProperty (SourceIDs::reverbRoomSize, SourceDefaults::reverbRoomSize, nullptr);
        return preset;
    }

    inline juce::ValueTree createAnalysisFromSlices(juce::StringArray slices)
    {
        juce::ValueTree soundAnalysis = juce::ValueTree(SourceIDs::ANALYSIS);
        juce::ValueTree soundAnalysisOnsetTimes = juce::ValueTree(SourceIDs::onsets);
        for (auto sliceString: slices){
            juce::ValueTree onset = juce::ValueTree(SourceIDs::onset);
            onset.setProperty(SourceIDs::onsetTime, sliceString.getFloatValue(), nullptr);
            soundAnalysisOnsetTimes.appendChild(onset, nullptr);
        }
        soundAnalysis.appendChild(soundAnalysisOnsetTimes, nullptr);
        return soundAnalysis;
    }

    inline juce::ValueTree createSourceSampleSoundState(int soundID,
                                                        const juce::String& soundName,
                                                        const juce::String& soundUser,
                                                        const juce::String& soundLicense,
                                                        const juce::String& previewURL,
                                                        const juce::String& localFilePath,
                                                        const juce::String& format,
                                                        int sizeBytes,
                                                        juce::StringArray slices,
                                                        int midiRootNote,
                                                        int midiVelocityLayer)
    {
        juce::ValueTree ss (SourceIDs::SOUND_SAMPLE);
        SourceHelpers::createUuidProperty (ss);
        ss.setProperty (SourceIDs::name, soundName, nullptr);
        ss.setProperty (SourceIDs::downloadProgress, 0, nullptr);
        ss.setProperty (SourceIDs::downloadCompleted, false, nullptr);
        ss.setProperty (SourceIDs::soundId, soundID, nullptr); // This might be -1 if sound is not from freesound
        ss.setProperty(SourceIDs::username, soundUser, nullptr);  // This might be "" if sound is not from freesound
        ss.setProperty(SourceIDs::license, soundLicense, nullptr);  // This might be "" if sound is not from freesound
        ss.setProperty (SourceIDs::format, format, nullptr);
        ss.setProperty (SourceIDs::previewURL, previewURL, nullptr); // This might be "" if sound is not from freesound
        ss.setProperty (SourceIDs::filePath, localFilePath, nullptr);
        ss.setProperty (SourceIDs::duration, -1.0, nullptr);  // This will be set when sound is loaded
        ss.setProperty (SourceIDs::filesize, sizeBytes, nullptr);  // This might be -1 if sound is not from freesound
        ss.setProperty (SourceIDs::soundFromFreesound, soundID > -1, nullptr);
        ss.setProperty (SourceIDs::usesPreview, false, nullptr);
        if (midiRootNote > -1){
            ss.setProperty (SourceIDs::midiRootNote, midiRootNote, nullptr);
        } else {
            ss.setProperty (SourceIDs::midiRootNote, SourceDefaults::midiRootNote, nullptr);
        }
        if (slices.size() > 0){
            juce::ValueTree soundAnalysis = SourceHelpers::createAnalysisFromSlices(slices);
            ss.addChild(soundAnalysis, -1, nullptr);
        }
        ss.setProperty(SourceIDs::midiVelocityLayer, midiVelocityLayer, nullptr);
        return ss;
    }

    inline juce::ValueTree createMidiMappingState(int ccNumber, juce::String parameterName, float minRange, float maxRange)
    {
        juce::ValueTree mapping (SourceIDs::MIDI_CC_MAPPING);
        SourceHelpers::createUuidProperty (mapping);
        mapping.setProperty (SourceIDs::ccNumber, ccNumber, nullptr);
        mapping.setProperty (SourceIDs::parameterName, parameterName, nullptr);
        mapping.setProperty (SourceIDs::minRange, minRange, nullptr);
        mapping.setProperty (SourceIDs::maxRange, maxRange, nullptr);
        return mapping;
    }

    inline juce::ValueTree createEmptySourceSoundState()
    {
        juce::ValueTree sound (SourceIDs::SOUND);
        SourceHelpers::createUuidProperty (sound);
        sound.setProperty (SourceIDs::willBeDeleted, false, nullptr);
        sound.setProperty (SourceIDs::allSoundsLoaded, false, nullptr);
        // --> Start auto-generated code A
        sound.setProperty (SourceIDs::launchMode, 0, nullptr);
        sound.setProperty (SourceIDs::startPosition, 0.0f, nullptr);
        sound.setProperty (SourceIDs::endPosition, 1.0f, nullptr);
        sound.setProperty (SourceIDs::loopStartPosition, 0.0f, nullptr);
        sound.setProperty (SourceIDs::loopEndPosition, 1.0f, nullptr);
        sound.setProperty (SourceIDs::loopXFadeNSamples, 500, nullptr);
        sound.setProperty (SourceIDs::reverse, 0, nullptr);
        sound.setProperty (SourceIDs::noteMappingMode, 0, nullptr);
        sound.setProperty (SourceIDs::numSlices, 0, nullptr);
        sound.setProperty (SourceIDs::playheadPosition, 0.0f, nullptr);
        sound.setProperty (SourceIDs::freezePlayheadSpeed, 100.0f, nullptr);
        sound.setProperty (SourceIDs::filterCutoff, 20000.0f, nullptr);
        sound.setProperty (SourceIDs::filterRessonance, 0.0f, nullptr);
        sound.setProperty (SourceIDs::filterKeyboardTracking, 0.0f, nullptr);
        sound.setProperty (SourceIDs::filterAttack, 0.01f, nullptr);
        sound.setProperty (SourceIDs::filterDecay, 0.0f, nullptr);
        sound.setProperty (SourceIDs::filterSustain, 1.0f, nullptr);
        sound.setProperty (SourceIDs::filterRelease, 0.01f, nullptr);
        sound.setProperty (SourceIDs::filterADSR2CutoffAmt, 1.0f, nullptr);
        sound.setProperty (SourceIDs::gain, -10.0f, nullptr);
        sound.setProperty (SourceIDs::attack, 0.01f, nullptr);
        sound.setProperty (SourceIDs::decay, 0.0f, nullptr);
        sound.setProperty (SourceIDs::sustain, 1.0f, nullptr);
        sound.setProperty (SourceIDs::release, 0.01f, nullptr);
        sound.setProperty (SourceIDs::pan, 0.0f, nullptr);
        sound.setProperty (SourceIDs::pitch, 0.0f, nullptr);
        sound.setProperty (SourceIDs::pitchBendRangeUp, 12.0f, nullptr);
        sound.setProperty (SourceIDs::pitchBendRangeDown, 12.0f, nullptr);
        sound.setProperty (SourceIDs::mod2CutoffAmt, 10.0f, nullptr);
        sound.setProperty (SourceIDs::mod2GainAmt, 6.0f, nullptr);
        sound.setProperty (SourceIDs::mod2PitchAmt, 0.0f, nullptr);
        sound.setProperty (SourceIDs::mod2PlayheadPos, 0.0f, nullptr);
        sound.setProperty (SourceIDs::vel2CutoffAmt, 0.0f, nullptr);
        sound.setProperty (SourceIDs::vel2GainAmt, 0.5f, nullptr);
        sound.setProperty (SourceIDs::velSensitivity, 1.0f, nullptr);
        sound.setProperty (SourceIDs::midiChannel, 0, nullptr);
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
        juce::ValueTree sourceSound = SourceHelpers::createEmptySourceSoundState();
        juce::ValueTree sourceSamplerSound = SourceHelpers::createSourceSampleSoundState(soundID,
                                                                                   soundName,
                                                                                   soundUser,
                                                                                   soundLicense,
                                                                                   previewURL,
                                                                                   localFilePath,
                                                                                   format,
                                                                                   sizeBytes,
                                                                                   slices,
                                                                                   midiRootNote,
                                                                                   midiVelocityLayer);

        if (!midiNotes.isZero()){
            // Note that MIDI notes are stored in Sound (not SourceSamplerSound)
            sourceSound.setProperty(SourceIDs::midiNotes, midiNotes.toString(16), nullptr);
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
        SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::ccNumber, SourceDefaults::ccNumber);
        ccNumber.referTo(state, SourceIDs::ccNumber, nullptr, SourceDefaults::ccNumber);
        SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::minRange, SourceDefaults::minRange);
        minRange.referTo(state, SourceIDs::minRange, nullptr, SourceDefaults::minRange);
        SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::maxRange, SourceDefaults::maxRange);
        maxRange.referTo(state, SourceIDs::maxRange, nullptr, SourceDefaults::maxRange);
        SourceHelpers::addPropertyWithDefaultValueIfNotExisting(state, SourceIDs::parameterName, SourceDefaults::parameterName);
        parameterName.referTo(state, SourceIDs::parameterName, nullptr);
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
        return v.hasType (SourceIDs::MIDI_CC_MAPPING);
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
        parent.removeChild(parent.getChildWithProperty(SourceIDs::uuid, uuid), nullptr);
    }
};
