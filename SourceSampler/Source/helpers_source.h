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
#include <limits>


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
        state.setProperty (SourceIDs::mpeEnabled, SourceDefaults::mpeEnabled, nullptr);
        state.setProperty (SourceIDs::mpePitchBendRange, SourceDefaults::mpePitchBendRange, nullptr);
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
        state.setProperty (SourceIDs::mpeEnabled, currentState.getProperty(SourceIDs::mpeEnabled, SourceDefaults::mpeEnabled), nullptr);
        state.setProperty (SourceIDs::mpePitchBendRange, currentState.getProperty(SourceIDs::mpePitchBendRange, SourceDefaults::mpePitchBendRange), nullptr);
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

    inline juce::ValueTree createMpeMappingState(const juce::String& source, const juce::String& parameterName, float minRange, float maxRange, float curve)
    {
        juce::ValueTree mapping (SourceIDs::MPE_MAPPING);
        SourceHelpers::createUuidProperty (mapping);
        mapping.setProperty (SourceIDs::mpeSource, source, nullptr);
        mapping.setProperty (SourceIDs::parameterName, parameterName, nullptr);
        mapping.setProperty (SourceIDs::minRange, minRange, nullptr);
        mapping.setProperty (SourceIDs::maxRange, maxRange, nullptr);
        mapping.setProperty (SourceIDs::curve, curve, nullptr);
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
        sound.setProperty (SourceIDs::pitchShift, 0.0f, nullptr);
        sound.setProperty (SourceIDs::timeStretch, 1.0f, nullptr);
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
                                                                                int midiVelocityLayer,
                                                                                int midiChannel){
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
        sourceSound.setProperty(SourceIDs::midiChannel, midiChannel, nullptr);
        
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

    //==============================================================================
    // Note layouts

    struct NoteLayout
    {
        juce::BigInteger midiNotes;
        int midiRootNote = SourceDefaults::midiRootNote;
        int midiChannel = SourceDefaults::midiChannel;
    };

    /** Computes the notes, root note and MIDI channel assigned to the sound at position soundIndex
        when numSounds sounds are laid out over the keyboard using the given layout type
        (NOTE_MAPPING_TYPE_CONTIGUOUS, NOTE_MAPPING_TYPE_INTERLEAVED or NOTE_MAPPING_TYPE_ALL). */
    inline NoteLayout computeNoteLayout (int noteLayoutType, int soundIndex, int numSounds)
    {
        NoteLayout layout;
        if (numSounds <= 0 || soundIndex < 0 || soundIndex >= numSounds)
            return layout;

        if (noteLayoutType == NOTE_MAPPING_TYPE_CONTIGUOUS){
            // All the notes mapped to this sound are contiguous in a range which depends on the total number of sounds
            const int nNotesPerSound = juce::jmax (1, 128 / numSounds);
            const int firstNote = juce::jmin (127, soundIndex * nNotesPerSound);
            layout.midiRootNote = juce::jmin (127, firstNote + nNotesPerSound / 2);
            layout.midiNotes.setRange (firstNote, juce::jmin (nNotesPerSound, 128 - firstNote), true);
        } else if (noteLayoutType == NOTE_MAPPING_TYPE_INTERLEAVED){
            // Notes are mapped to sounds in interleaved fashion so each contiguous note corresponds to a different sound
            layout.midiRootNote = NOTE_MAPPING_INTERLEAVED_ROOT_NOTE + soundIndex;
            if (layout.midiRootNote > 127)
                layout.midiRootNote = NOTE_MAPPING_INTERLEAVED_ROOT_NOTE + (soundIndex % (128 - NOTE_MAPPING_INTERLEAVED_ROOT_NOTE));
            for (int j = layout.midiRootNote; j < 128; j += numSounds)
                layout.midiNotes.setBit (j);  // Map notes in upwards direction
            for (int j = layout.midiRootNote; j >= 0; j -= numSounds)
                layout.midiNotes.setBit (j);  // Map notes in downwards direction
        } else if (noteLayoutType == NOTE_MAPPING_TYPE_ALL){
            // Map all notes to all sounds, each sound listening on a different MIDI channel
            layout.midiNotes.setRange (0, 128, true);
            layout.midiRootNote = 64;
            layout.midiChannel = (soundIndex % 16) + 1;
        }
        return layout;
    }

    /** Returns the "index" of midiNote within the set of mapped notes, ignoring gaps in the mapping.
        For example, with notes {C3, C#3, E3} mapped, E3 has index 2. If midiNote is not mapped, the
        index of the closest mapped note is returned. Returns 0 if no notes are mapped. */
    inline int noteIndexInMapping (const juce::BigInteger& mappedMidiNotes, int midiNote)
    {
        int noteIndex = 0;
        int closestNoteIndex = 0;
        int minDistance = std::numeric_limits<int>::max();
        for (int note = mappedMidiNotes.findNextSetBit (0); note >= 0; note = mappedMidiNotes.findNextSetBit (note + 1)){
            if (note == midiNote)
                return noteIndex;
            const int distance = std::abs (note - midiNote);
            if (distance < minDistance){
                minDistance = distance;
                closestNoteIndex = noteIndex;
            }
            noteIndex += 1;
        }
        return closestNoteIndex;
    }

    //==============================================================================
    // MPE

    enum class MpeSource { press = 0, slide, glide, strike, numSources };

    inline juce::StringArray getMpeSourceNames()
    {
        return { MPE_SOURCE_PRESS, MPE_SOURCE_SLIDE, MPE_SOURCE_GLIDE, MPE_SOURCE_STRIKE };
    }

    /** Returns the source with the given name, or MpeSource::numSources if the name is not valid. */
    inline MpeSource mpeSourceFromName (const juce::String& name)
    {
        const int index = getMpeSourceNames().indexOf (name);
        return index >= 0 ? (MpeSource) index : MpeSource::numSources;
    }

    inline bool isBipolarMpeSource (MpeSource source) { return source == MpeSource::glide; }

    /** Applies a response curve to a normalised (0..1) MPE source value. curve is in [-1, 1]: 0 is linear,
        positive values make the response slower at the start (exponential-like), negative values faster (log-like).
        Bipolar sources (glide, where 0.5 is the centre) apply the curve symmetrically around the centre. */
    inline float applyMpeCurve (float value01, float curve, bool bipolar)
    {
        value01 = juce::jlimit (0.0f, 1.0f, value01);
        curve = juce::jlimit (-1.0f, 1.0f, curve);
        if (curve == 0.0f)
            return value01;
        const float exponent = std::pow (4.0f, curve);
        if (! bipolar)
            return std::pow (value01, exponent);
        const float centred = value01 * 2.0f - 1.0f;
        const float shaped = std::copysign (std::pow (std::abs (centred), exponent), centred);
        return (shaped + 1.0f) * 0.5f;
    }

    /** Maps a (curved) normalised source value to a parameter value: first to the mapping's normalised
        [minRange, maxRange] range, then to the parameter's [parameterMin, parameterMax] range. */
    inline float mapMpeValueToParameter (float value01, float curve, bool bipolar, float minRange, float maxRange, float parameterMin, float parameterMax)
    {
        const float normalised = juce::jmap (applyMpeCurve (value01, curve, bipolar), minRange, maxRange);
        return juce::jmap (normalised, parameterMin, parameterMax);
    }

    //==============================================================================
    // Action message (internal API) builders. All UIs (web, Python, native) control the sampler by sending
    // these messages to SourceSampler::actionListenerCallback.

    inline juce::String serializeAction (const juce::String& actionName, const juce::StringArray& parameters = {})
    {
        return actionName + ":" + parameters.joinIntoString (SERIALIZATION_SEPARATOR);
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
