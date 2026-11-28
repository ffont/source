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
#include <complex>


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
    /** Base folder where the SOURCE data folder is created when no folder is given explicitly: the shared app group
        container on iOS (if available), otherwise the user's documents folder. */
    inline juce::File getDefaultBaseDataLocation()
    {
        #if USE_APP_GROUP_ID
        auto container = juce::File::getContainerForSecurityApplicationGroupIdentifier (APP_GROUP_ID);
        if (container != juce::File() && container.isDirectory())
            return container;
        #endif
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
    }

    /** True if downloads can be written to the shared app group container (iOS). */
    inline bool canUseSharedContainerForDownloads()
    {
        #if USE_APP_GROUP_ID
        auto container = juce::File::getContainerForSecurityApplicationGroupIdentifier (APP_GROUP_ID);
        return container != juce::File() && container.isDirectory();
        #else
        return false;
        #endif
    }


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

    /** Manual slices (times in seconds) as stored in the "slices" property of sounds, and back. */
    /** Magnitude response (linear gain) of the sounds' filter (juce::dsp::LadderFilter in its default LPF12 mode, with
        its default drive of 1.2, for small signals) at the given frequency. Computed from the filter's difference
        equations: four one-pole stages H1(z) = g (0.769 + 0.231 z^-1) / (1 - a1 z^-1), with a1 = exp(-2 pi fc / fs) and
        g = 1 - a1, and the input a = dx - 4 k (d2 z^-1 e - 0.5 dx), where dx = d1 x, k = 0.1 + 0.9 resonance, e is the
        output of the fourth stage and d1, d2 are the small-signal gains of the drive stages. The output is 1.2 times
        the second stage's. */
    inline double ladderFilterMagnitude (double frequencyHz, double cutoffHz, double resonance, double sampleRate)
    {
        using C = std::complex<double>;
        const double a1 = std::exp (-2.0 * juce::MathConstants<double>::pi * juce::jmax (0.001, cutoffHz) / sampleRate);
        const double g = 1.0 - a1;
        const double k = 0.1 + 0.9 * juce::jlimit (0.0, 1.0, resonance);
        const C zInv = std::polar (1.0, -2.0 * juce::MathConstants<double>::pi * juce::jlimit (0.0, sampleRate * 0.5, frequencyHz) / sampleRate);
        const C h1 = g * (0.76923076923 + 0.23076923076 * zInv) / (1.0 - a1 * zInv);
        const C h2 = h1 * h1;
        auto driveGain = [] (double drive) { return drive * (std::pow (drive, -2.642) * 0.6103 + 0.3903); };
        const double d1 = driveGain (1.2), d2 = driveGain (1.2 * 0.04 + 0.96);
        const C a = d1 * (1.0 + 2.0 * k) / (1.0 + 4.0 * k * d2 * zInv * h2 * h2);
        return std::abs (1.2 * h2 * a);
    }

    /** Level (0..1) of an ADSR envelope (linear segments, like juce::ADSR) at a time (seconds) after note on, for a
        note released at releaseTime (seconds after note on). */
    inline float adsrLevelAt (float time, float attack, float decay, float sustain, float release, float releaseTime)
    {
        auto levelBeforeRelease = [&] (float t)
        {
            if (t < attack)
                return attack > 0.0f ? t / attack : 1.0f;
            if (t < attack + decay)
                return 1.0f - (1.0f - sustain) * (decay > 0.0f ? (t - attack) / decay : 1.0f);
            return sustain;
        };
        if (time < releaseTime)
            return levelBeforeRelease (time);
        const float releaseStartLevel = levelBeforeRelease (releaseTime);
        if (release <= 0.0f)
            return 0.0f;
        return juce::jmax (0.0f, releaseStartLevel * (1.0f - (time - releaseTime) / release));
    }

    inline std::vector<float> parseSliceTimes (const juce::String& text)
    {
        std::vector<float> times;
        for (const auto& token : juce::StringArray::fromTokens (text, ",", {}))
            if (token.trim().isNotEmpty())
                times.push_back (token.getFloatValue());
        return times;
    }
    inline juce::String formatSliceTimes (const std::vector<float>& times)
    {
        juce::StringArray tokens;
        for (auto t : times)
            tokens.add (juce::String (t, 4));
        return tokens.joinIntoString (",");
    }
    /** Sorts the times, removes duplicates, negative times and the ones too close to the start (which would make
        empty slices), and keeps at most MAX_MANUAL_SLICES. */
    inline std::vector<float> cleanSliceTimes (std::vector<float> times, float minTime = 0.005f)
    {
        std::sort (times.begin(), times.end());
        std::vector<float> cleaned;
        for (auto t : times)
            if (t >= minTime && (cleaned.empty() || t - cleaned.back() > 0.0005f) && (int) cleaned.size() < MAX_MANUAL_SLICES)
                cleaned.push_back (t);
        return cleaned;
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

    inline juce::ValueTree createModulatorState(const juce::String& type, const juce::String& parameterName, float minRange, float maxRange,
                                                int lfoWaveform, float lfoRate, float attack, float decay, float sustain, float release)
    {
        juce::ValueTree modulator (SourceIDs::MODULATOR);
        SourceHelpers::createUuidProperty (modulator);
        modulator.setProperty (SourceIDs::modulatorType, type, nullptr);
        modulator.setProperty (SourceIDs::parameterName, parameterName, nullptr);
        modulator.setProperty (SourceIDs::minRange, minRange, nullptr);
        modulator.setProperty (SourceIDs::maxRange, maxRange, nullptr);
        modulator.setProperty (SourceIDs::lfoWaveform, lfoWaveform, nullptr);
        modulator.setProperty (SourceIDs::lfoRate, lfoRate, nullptr);
        modulator.setProperty (SourceIDs::modAttack, attack, nullptr);
        modulator.setProperty (SourceIDs::modDecay, decay, nullptr);
        modulator.setProperty (SourceIDs::modSustain, sustain, nullptr);
        modulator.setProperty (SourceIDs::modRelease, release, nullptr);
        return modulator;
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
        sourceSound.setProperty(SourceIDs::slices, "", nullptr);  // New sounds start without manual slices (see SourceSound::bindState)
        
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

    enum class MpeSource { slide = 0, glide, strike, numSources };

    inline juce::StringArray getMpeSourceNames()
    {
        return { MPE_SOURCE_SLIDE, MPE_SOURCE_GLIDE, MPE_SOURCE_STRIKE };
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
    // Modulators: internal signals (LFO, ADSR) that modulate a sound parameter, independently for each voice.
    // A modulator's signal goes from 0 to 1 and its output is jmap (signal, minRange, maxRange), an offset added to
    // the target parameter in its normalised (0..1) range. With minRange = maxRange = 0 (the default) there is no
    // modulation.

    enum class ModulatorType { lfo = 0, adsr, steps, aftertouch, velocity, numTypes };

    inline juce::StringArray getModulatorTypeNames()
    {
        return { MODULATOR_TYPE_LFO, MODULATOR_TYPE_ADSR, MODULATOR_TYPE_STEPS, MODULATOR_TYPE_AFTERTOUCH, MODULATOR_TYPE_VELOCITY };
    }

    /** Returns the type with the given name, or ModulatorType::numTypes if the name is not valid. */
    inline ModulatorType modulatorTypeFromName (const juce::String& name)
    {
        const int index = getModulatorTypeNames().indexOf (name);
        return index >= 0 ? (ModulatorType) index : ModulatorType::numTypes;
    }

    enum LfoWaveform { lfoSine = 0, lfoTriangle, lfoSawUp, lfoSawDown, lfoSquare, lfoRandom, numLfoWaveforms };

    inline juce::StringArray getLfoWaveformNames()
    {
        return { "Sine", "Triangle", "Saw up", "Saw down", "Square", "Random" };
    }

    constexpr float minLfoRate = 0.01f, maxLfoRate = 30.0f;  // Hz
    constexpr float maxModulatorEnvelopeTime = 10.0f;  // Seconds
    constexpr double minBpm = 20.0, maxBpm = 300.0;

    /** Note divisions for synced LFOs (rate: one LFO cycle per division; attack: the attack lasts one division):
        bars and binary divisions, dotted (3/2) and triplet (2/3) relations. Length in beats (quarter notes). */
    struct SyncDivision
    {
        const char* name;
        double beats;
    };
    inline const std::vector<SyncDivision>& getSyncDivisions()
    {
        static const std::vector<SyncDivision> divisions {
            { "8 bars", 32.0 }, { "4 bars", 16.0 }, { "2 bars", 8.0 }, { "1 bar", 4.0 },
            { "1/2 D", 3.0 }, { "1/2", 2.0 }, { "1/2 T", 4.0 / 3.0 },
            { "1/4 D", 1.5 }, { "1/4", 1.0 }, { "1/4 T", 2.0 / 3.0 },
            { "1/8 D", 0.75 }, { "1/8", 0.5 }, { "1/8 T", 1.0 / 3.0 },
            { "1/16 D", 0.375 }, { "1/16", 0.25 }, { "1/16 T", 1.0 / 6.0 },
            { "1/32", 0.125 }, { "1/32 T", 1.0 / 12.0 },
        };
        return divisions;
    }
    inline double getSyncDivisionBeats (int index)
    {
        const auto& divisions = getSyncDivisions();
        return divisions[(size_t) juce::jlimit (0, (int) divisions.size() - 1, index)].beats;
    }
    /** Synced LFO attack choices: "Off" and then the sync divisions. */
    inline juce::StringArray getSyncAttackNames()
    {
        juce::StringArray names { "Off" };
        for (const auto& division : getSyncDivisions())
            names.add (division.name);
        return names;
    }
    inline double getSyncAttackBeats (int index)
    {
        return index <= 0 ? 0.0 : getSyncDivisionBeats (index - 1);
    }

    //==============================================================================
    // Steps modulators: a sequence of values (0..1) cycled at a rate (whole cycles per second) or, synced, in a cycle
    // length of beats/bars.

    constexpr int maxSteps = 32;
    constexpr float minStepsRate = 0.005f, maxStepsRate = 10.0f;  // Cycles per second

    /** Synced steps: length of the whole cycle (binary and ternary relations), in beats. */
    inline const std::vector<SyncDivision>& getStepsCycleLengths()
    {
        static const std::vector<SyncDivision> lengths {
            { "1/2 beat", 0.5 }, { "1 beat", 1.0 }, { "2 beats", 2.0 }, { "3 beats", 3.0 }, { "1 bar", 4.0 },
            { "2 bars", 8.0 }, { "3 bars", 12.0 }, { "4 bars", 16.0 }, { "6 bars", 24.0 }, { "8 bars", 32.0 },
            { "12 bars", 48.0 }, { "16 bars", 64.0 }, { "32 bars", 128.0 },
        };
        return lengths;
    }
    inline double getStepsCycleBeats (int index)
    {
        const auto& lengths = getStepsCycleLengths();
        return lengths[(size_t) juce::jlimit (0, (int) lengths.size() - 1, index)].beats;
    }

    using StepValues = std::array<float, (size_t) maxSteps>;

    /** Default step values: a ramp over 8 steps, repeated. */
    inline StepValues getDefaultStepValues()
    {
        StepValues values {};
        for (int i = 0; i < maxSteps; ++i)
            values[(size_t) i] = (float) (i % 8) / 7.0f;
        return values;
    }

    /** Parses comma-separated step values (missing values take the defaults, values are clamped to 0..1). */
    inline StepValues parseStepValues (const juce::String& text)
    {
        auto values = getDefaultStepValues();
        const auto tokens = juce::StringArray::fromTokens (text, ",", {});
        for (int i = 0; i < juce::jmin (tokens.size(), maxSteps); ++i)
            if (tokens[i].trim().isNotEmpty())
                values[(size_t) i] = juce::jlimit (0.0f, 1.0f, tokens[i].getFloatValue());
        return values;
    }

    inline juce::String formatStepValues (const StepValues& values)
    {
        juce::StringArray tokens;
        for (auto v : values)
            tokens.add (juce::String (v, 3));
        return tokens.joinIntoString (",");
    }

    /** Value of the step at the given phase of the cycle [0, 1). */
    inline float stepsSignal (const StepValues& values, int numSteps, double phase) noexcept
    {
        numSteps = juce::jlimit (1, maxSteps, numSteps);
        const double p = phase - std::floor (phase);
        const int index = juce::jlimit (0, numSteps - 1, (int) (p * numSteps));
        return values[(size_t) index];
    }

    /** LFO attack: the LFO rate goes from 0 Hz to its rate following an exponential curve (slow start, fast end).
        Returns the factor (0..1) to apply to the rate after elapsedSeconds. */
    inline double lfoAttackRateFactor (double elapsedSeconds, double attackSeconds) noexcept
    {
        if (attackSeconds <= 0.0 || elapsedSeconds >= attackSeconds)
            return 1.0;
        constexpr double k = 5.0;  // Curvature
        const double x = juce::jmax (0.0, elapsedSeconds / attackSeconds);
        return (std::exp (k * x) - 1.0) / (std::exp (k) - 1.0);
    }

    /** LFO signal (0..1) for a phase in [0, 1). All waveforms start at 0 (sine and triangle start at their minimum,
        so a note always starts at the modulator's min value). randomValue is the value held during this cycle
        (random waveform). */
    inline float lfoSignal (int waveform, double phase, float randomValue) noexcept
    {
        const float p = (float) (phase - std::floor (phase));
        switch (waveform)
        {
            case lfoSine:     return 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * p);
            case lfoTriangle: return p < 0.5f ? 2.0f * p : 2.0f - 2.0f * p;
            case lfoSawUp:    return p;
            case lfoSawDown:  return 1.0f - p;
            case lfoSquare:   return p < 0.5f ? 1.0f : 0.0f;
            case lfoRandom:   return juce::jlimit (0.0f, 1.0f, randomValue);
            default:          return 0.0f;
        }
    }

    /** Linear ADSR envelope (0..1) advanced in time steps (used by ADSR modulators, per voice). */
    struct ModulatorEnvelope
    {
        enum class Stage { idle, attack, decay, sustain, release };
        Stage stage = Stage::idle;
        float level = 0.0f;
        float releaseStartLevel = 0.0f;

        void noteOn() noexcept { stage = Stage::attack; level = 0.0f; }
        void noteOff() noexcept
        {
            if (stage != Stage::idle && stage != Stage::release)
            {
                stage = Stage::release;
                releaseStartLevel = level;
            }
        }
        void reset() noexcept { stage = Stage::idle; level = 0.0f; }

        /** Advances the envelope by seconds and returns the new level. */
        float advance (float seconds, float attack, float decay, float sustain, float release) noexcept
        {
            constexpr float minTime = 0.0005f;
            sustain = juce::jlimit (0.0f, 1.0f, sustain);
            while (seconds > 0.0f)
            {
                switch (stage)
                {
                    case Stage::idle:
                        level = 0.0f;
                        return level;
                    case Stage::attack:
                    {
                        const float time = juce::jmax (minTime, attack);
                        const float remaining = (1.0f - level) * time;  // Seconds left to reach 1
                        if (seconds < remaining) { level += seconds / time; return level; }
                        seconds -= remaining;
                        level = 1.0f;
                        stage = Stage::decay;
                        break;
                    }
                    case Stage::decay:
                    {
                        const float time = juce::jmax (minTime, decay);
                        const float slope = (1.0f - sustain) / time;  // Per second
                        if (slope <= 0.0f || level <= sustain) { level = sustain; stage = Stage::sustain; break; }
                        const float remaining = (level - sustain) / slope;
                        if (seconds < remaining) { level -= seconds * slope; return level; }
                        seconds -= remaining;
                        level = sustain;
                        stage = Stage::sustain;
                        break;
                    }
                    case Stage::sustain:
                        level = sustain;
                        return level;
                    case Stage::release:
                    {
                        const float time = juce::jmax (minTime, release);
                        const float slope = juce::jmax (releaseStartLevel, 0.0001f) / time;
                        level -= seconds * slope;
                        if (level <= 0.0f) { level = 0.0f; stage = Stage::idle; }
                        return level;
                    }
                }
            }
            return level;
        }
    };

    /** Adds a modulation offset (in the parameter's normalised range) to a parameter value. */
    inline float applyModulationOffset (float value, float normalisedOffset, float parameterMin, float parameterMax) noexcept
    {
        return juce::jlimit (parameterMin, parameterMax, value + normalisedOffset * (parameterMax - parameterMin));
    }

    //==============================================================================
    // Action message (internal API) builders. All UIs (web, Python, native) control the sampler by sending
    // these messages to SourceSampler::actionListenerCallback.

    /** Short name for a Creative Commons license URL, e.g. "http://creativecommons.org/licenses/by-nc/3.0/" -> "CC BY-NC 3.0". */
    inline juce::String getShortLicenseName (const juce::String& licenseURL)
    {
        if (licenseURL.contains ("publicdomain/zero"))
            return "CC0";
        if (licenseURL.contains ("sampling+"))
            return "CC Sampling+";
        if (licenseURL.contains ("/licenses/"))
        {
            auto parts = juce::StringArray::fromTokens (licenseURL.fromFirstOccurrenceOf ("/licenses/", false, false), "/", {});
            parts.removeEmptyStrings();
            if (parts.size() >= 1)
                return "CC " + parts[0].toUpperCase() + (parts.size() >= 2 ? " " + parts[1] : juce::String());
        }
        return licenseURL;
    }

    /** Freesound previews moved from freesound.org/data/previews/ to cdn.freesound.org/previews/. The old URLs (stored
        in old presets) still work but redirect, and the redirect adds a request to every download. Returns the URL
        pointing to the CDN (other URLs are returned unchanged). */
    inline juce::String fixFreesoundPreviewURL (const juce::String& url)
    {
        for (auto oldPrefix : { "https://freesound.org/data/previews/", "http://freesound.org/data/previews/",
                                "https://www.freesound.org/data/previews/", "http://www.freesound.org/data/previews/" })
            if (url.startsWith (oldPrefix))
                return "https://cdn.freesound.org/previews/" + url.substring ((int) strlen (oldPrefix));
        return url;
    }

    /** Rewrites the old Freesound preview URLs in a preset (or session) file to the CDN ones (see
        fixFreesoundPreviewURL). Returns true if the file was changed. */
    inline bool fixFreesoundPreviewURLsInFile (const juce::File& file)
    {
        const auto text = file.loadFileAsString();
        auto fixed = text;
        for (auto oldPrefix : { "https://freesound.org/data/previews/", "http://freesound.org/data/previews/",
                                "https://www.freesound.org/data/previews/", "http://www.freesound.org/data/previews/" })
            fixed = fixed.replace (oldPrefix, "https://cdn.freesound.org/previews/");
        if (fixed == text)
            return false;
        return file.replaceWithText (fixed, false, false, nullptr);  // Keep the line endings of the file
    }

    /** Reads the name of a preset file without parsing the whole file (preset files can be large): only the root
        element's start tag is parsed. Works with current ("PRESET", name attribute) and old ("SourcePresetState",
        presetName attribute) preset files. Returns an empty string if the file is not a readable preset. */
    inline juce::String readPresetNameFromFile (const juce::File& file)
    {
        juce::FileInputStream in (file);
        if (! in.openedOk())
            return {};
        juce::MemoryBlock data;
        in.readIntoMemoryBlock (data, 16384);
        const auto text = data.toString();

        // Find the root element, skipping the XML declaration, comments and doctype
        int start = 0;
        for (;;)
        {
            start = text.indexOfChar (start, '<');
            if (start < 0 || start + 1 >= text.length())
                return {};
            const auto next = text[start + 1];
            if (next != '?' && next != '!')
                break;
            start = text.indexOfChar (start, '>');
            if (start < 0)
                return {};
        }
        const int end = text.indexOfChar (start, '>');
        if (end < 0)
            return {};
        auto startTag = text.substring (start, end).trimEnd();
        if (startTag.endsWithChar ('/'))
            startTag = startTag.dropLastCharacters (1);
        if (auto element = juce::XmlDocument::parse (startTag + "/>"))
        {
            if (element->hasTagName ("PRESET"))
                return element->getStringAttribute ("name");
            if (element->hasTagName ("SourcePresetState"))
                return element->getStringAttribute ("presetName");
        }
        return {};
    }

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
