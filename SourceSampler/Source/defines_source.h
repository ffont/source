/*
  ==============================================================================

    defines.h
    Created: 2 Sep 2020 6:20:58pm
    Author:  Frederic Font Corbera

  ==============================================================================
*/

#pragma once

#ifndef INCLUDE_SEQUENCER
#define INCLUDE_SEQUENCER 0
#endif

#define USE_OSC_SERVER 0  // OSC can be used as well as an alternative to Websockets, keeping this here for legacy reasons but not really used
#define SYNC_STATE_WITH_OSC 0

#if ELK_BUILD
    #define USE_HTTP_SERVER 1 // Use the embedded http server to serve audio files because even though in python ui we direcly load them form disk, if loading the web ui we still want to be able to see waveforms
    #define USE_EXTERNAL_HTTP_SERVER_FOR_DOWNLOADS 1  // In ELK, downloads also happen through the external HTTP server
#else
    #define USE_HTTP_SERVER 1  // Use embedded http server to serve audio files to plugin UI
    #define USE_EXTERNAL_HTTP_SERVER_FOR_DOWNLOADS 0  // Don't use external HTTP server for downloads, download them from the plugin
#endif

#if SOURCE_DISABLE_HTTP_SERVER
#undef USE_HTTP_SERVER
#define USE_HTTP_SERVER 0
#endif

#ifndef USING_DIRECT_COMMUNICATION_METHOD
#define USING_DIRECT_COMMUNICATION_METHOD 0  // Direct communication method (without ws server) is implemented, but it does not seem to perform better than ws method. However, it removes the openSSL dependency which is great when compiling in iOS
#endif

#if !USING_DIRECT_COMMUNICATION_METHOD
#define USE_WS_SERVER 1  // If not using direct communication method, enable WebSockets server
#else
#define USE_WS_SERVER 0  // If using direct communicaiton method, don't enable WebSockets server
#endif


#define USE_SSL_FOR_HTTP_AND_WS 0 // At some point it looked like SSL was necessary for http and ws servers to work on macOS, but not it looks like it also works without that. The downside of using ssl is that the bundled self-certificate needs to be accepted on a browser before the UI can be loaded

#ifndef LOADED_LATEST_LOADED_PRESET_AT_STARTUP
#define LOADED_LATEST_LOADED_PRESET_AT_STARTUP 0
#endif

#if ELK_BUILD
#undef LOADED_LATEST_LOADED_PRESET_AT_STARTUP
#define LOADED_LATEST_LOADED_PRESET_AT_STARTUP 1
#endif

#define USE_LAZY_UI 0

#define MAIN_TIMER_HZ 15  // Run main timer tasks at this rate (this includes removing sounds that need to be removed and possibly other tasks)
#define SAFE_SOUND_DELETION_TIME_MS 200
#define SAMPLER_SOUND_TIMER_MS 20
#define STRETCH_PROCESSING_TIME_DEBOUNCE_MS 200.0

#ifndef SOURCE_APP_DIRECTORY_NAME
#define SOURCE_APP_DIRECTORY_NAME "SourceSampler"  // Note this is ignored in ELK builds
#endif
#define ELK_SOURCE_DATA_BASE_LOCATION "/udata/source/"
#define ELK_SOURCE_SOUNDS_LOCATION "/udata/source/sounds/"
#define ELK_SOURCE_PRESETS_LOCATION "/udata/source/presets/"
#define ELK_SOURCE_TMP_LOCATION "/tmp/source/"
#define USE_APP_GROUP_ID 1
#define APP_GROUP_ID "group.ritaiaurora.source"

#define OSC_LISTEN_PORT 9001
#define OSC_TO_SEND_PORT 9002  // OSC port where the glue app is listening
#define HTTP_SERVER_PORT 8124
#define WEBSOCKETS_SERVER_PORT 8125
#define HTTP_DOWNLOAD_SERVER_PORT 8123

#define MAX_SAMPLE_LENGTH 300  // minutes maximum sample length

#define MAX_DOWNLOAD_WAITING_TIME_MS 20000
#define MAX_SIZE_FOR_ORIGINAL_FILE_DOWNLOAD 1024 * 1024 * 15  // 15 MB

#define ENABLE_DEBUG_BUFFER 0  // User as a debugging trick for outputting some audio to a file


// Global actions
#define ACTION_GET_STATE "/get_state"
#define ACTION_PLAY_SOUND_FILE_FROM_PATH "/play_sound_from_path"
#define ACTION_SET_USE_ORIGINAL_FILES_PREFERENCE "/set_use_original_files"
#define ACTION_SET_FREESOUND_OAUTH_TOKEN "/set_oauth_token"
#define ACTION_SET_GLOBAL_MIDI_IN_CHANNEL "/set_midi_in_channel"
#define ACTION_NOTE_ON "/note_on"
#define ACTION_NOTE_OFF "/note_off"

// Preset actions
#define ACTION_LOAD_PRESET "/load_preset"
#define ACTION_SAVE_PRESET "/save_preset"
#define ACTION_SET_REVERB_PARAMETERS "/set_reverb_parameters"
#define ACTION_SET_POLYPHONY "/set_polyphony"
#define ACTION_REAPPLY_LAYOUT "/reapply_layout"
#define ACTION_CLEAR_ALL_SOUNDS "/clear_all_sounds"
#define ACTION_ADD_NEW_SOUNDS_FROM_QUERY "/add_sounds_from_query"
#define ACTION_REPLACE_EXISTING_SOUNDS_FROM_QUERY "/replace_sounds_from_query"
#define ACTION_REPLACE_SOUND_FROM_QUERY "/replace_sound_from_query"

// Sound actions
#define ACTION_ADD_SOUND "/add_sound"
#define ACTION_REPLACE_SOUND "/replace_sound"
#define ACTION_REMOVE_SOUND "/remove_sound"
#define ACTION_SET_SOUND_PARAMETER_FLOAT "/set_sound_parameter"
#define ACTION_SET_SOUND_PARAMETER_INT "/set_sound_parameter_int"
#define ACTION_SET_SOUND_SLICES "/set_slices"
#define ACTION_SET_SOUND_ASSIGNED_NOTES "/set_assigned_notes"
#define ACTION_PLAY_SOUND "/play_sound"
#define ACTION_STOP_SOUND "/stop_sound"

// SamplerSound actions
#define ACTION_ADD_SAMPLER_SOUND "/add_sampler_sound"
#define ACTION_REMOVE_SAMPLER_SOUND "/remove_sampler_sound"
#define ACTION_SET_SAMPLER_SOUND_PARAMETERS "/set_sampler_sound_parameters"
#define ACTION_SET_SAMPLER_SOUND_MIDI_ROOT_NOTE "/set_sampler_sound_root_note"

// MIDI CC mapping actions
#define ACTION_ADD_OR_UPDATE_CC_MAPPING "/add_or_update_cc_mapping"
#define ACTION_REMOVE_CC_MAPPING "/remove_cc_mapping"

// External sound downloads actions
#define ACTION_DOWNLOADING_SOUND_PROGRESS "/downloading_sound_progress"
#define ACTION_FINISHED_DOWNLOADING_SOUND "/finished_downloading_sound"


// "Enumneration" parameters definitions

#define LAUNCH_MODE_GATE 0
#define LAUNCH_MODE_LOOP 1
#define LAUNCH_MODE_LOOP_FW_BW 2
#define LAUNCH_MODE_TRIGGER 3
#define LAUNCH_MODE_FREEZE 4

#define SLICE_MODE_AUTO_NNOTES 1
#define SLICE_MODE_AUTO_ONSETS 0

#define NOTE_MAPPING_MODE_PITCH 0
#define NOTE_MAPPING_MODE_SLICE 1
#define NOTE_MAPPING_MODE_BOTH 2
#define NOTE_MAPPING_MODE_REPEAT 3

#define NOTE_MAPPING_TYPE_CONTIGUOUS 0  // Don't confuse note mapping type with note mapping mode. I know, naming should improve...
#define NOTE_MAPPING_TYPE_INTERLEAVED 1
#define NOTE_MAPPING_INTERLEAVED_ROOT_NOTE 36 // C2 (the note from which sounds start being mapped (also in backwards direction)
#define NOTE_MAPPING_TYPE_ALL 2  // Map all notes to "all" sounds

#define USE_ORIGINAL_FILES_NEVER "never"
#define USE_ORIGINAL_FILES_ONLY_SHORT "onlyShort"
#define USE_ORIGINAL_FILES_ALWAYS "always"

#define SERIALIZATION_SEPARATOR ";"


namespace SourceDefaults
{
#if INCLUDE_SEQUENCER
inline int noteLayoutType = NOTE_MAPPING_TYPE_ALL;
#else
inline int noteLayoutType = NOTE_MAPPING_TYPE_INTERLEAVED;
#endif
inline int currentPresetIndex = -1;
inline int globalMidiInChannel = 0;
inline int numVoices = 8;
inline bool midiOutForwardsMidiIn = false;
inline juce::String useOriginalFilesPreference = USE_ORIGINAL_FILES_NEVER;
inline juce::String freesoundOauthAccessToken = "";
inline int midiVelocityLayer = 0;
inline int midiRootNote = 64;
inline bool willBeDeleted = false;
inline bool allSoundsLoaded = false;
inline juce::String midiNotes = "ffffffffffffffffffffffffffffffff";
inline int soundId = -1;
inline juce::String name = "no name";

// --> Start auto-generated code A
inline int launchMode = 0;
inline float startPosition = 0.0f;
inline float endPosition = 1.0f;
inline float loopStartPosition = 0.0f;
inline float loopEndPosition = 1.0f;
inline int loopXFadeNSamples = 500;
inline int reverse = 0;
inline int noteMappingMode = 0;
inline int numSlices = 0;
inline float playheadPosition = 0.0f;
inline float freezePlayheadSpeed = 100.0f;
inline float filterCutoff = 20000.0f;
inline float filterRessonance = 0.0f;
inline float filterKeyboardTracking = 0.0f;
inline float filterAttack = 0.01f;
inline float filterDecay = 0.0f;
inline float filterSustain = 1.0f;
inline float filterRelease = 0.01f;
inline float filterADSR2CutoffAmt = 1.0f;
inline float gain = -10.0f;
inline float attack = 0.01f;
inline float decay = 0.0f;
inline float sustain = 1.0f;
inline float release = 0.01f;
inline float pan = 0.0f;
inline float pitch = 0.0f;
inline float pitchBendRangeUp = 12.0f;
inline float pitchBendRangeDown = 12.0f;
inline float mod2CutoffAmt = 10.0f;
inline float mod2GainAmt = 6.0f;
inline float mod2PitchAmt = 0.0f;
inline float mod2PlayheadPos = 0.0f;
inline float vel2CutoffAmt = 0.0f;
inline float vel2GainAmt = 0.5f;
inline float velSensitivity = 1.0f;
inline int midiChannel = 0;
inline float pitchShift = 0.0f;
inline float timeStretch = 1.0f;
// --> End auto-generated code A

inline float sampleStartPosition = -1.0f;
inline float sampleEndPosition = -1.0f;
inline float sampleLoopStartPosition = -1.0f;
inline float sampleLoopEndPosition = -1.0f;

inline float reverbRoomSize = 0.5f;
inline float reverbDamping = 0.3f;
inline float reverbWetLevel = 0.0f;
inline float reverbDryLevel = 1.0f;
inline float reverbWidth = 0.5f;
inline float reverbFreezeMode = 0.0f;

inline juce::String parameterName = "no name";
inline int ccNumber = -1;
inline float minRange = 0.0;
inline float maxRange = 127.0;
}


namespace SourceIDs
{
#define DECLARE_ID(name) const juce::Identifier name (#name);

DECLARE_ID (SOURCE_STATE)
DECLARE_ID (PRESET)
DECLARE_ID (SOUND)
DECLARE_ID (MIDI_CC_MAPPING)
DECLARE_ID (SOUND_SAMPLE)
DECLARE_ID (ANALYSIS)
DECLARE_ID (GLOBAL_SETTINGS)
DECLARE_ID (VOLATILE_STATE)

DECLARE_ID (filePath)
DECLARE_ID (downloadProgress)
DECLARE_ID (downloadCompleted)
DECLARE_ID (logMessages)
DECLARE_ID (isQuerying)

DECLARE_ID (uuid)
DECLARE_ID (name)
DECLARE_ID (soundId)
DECLARE_ID (previewURL)
DECLARE_ID (soundFromFreesound)
DECLARE_ID (usesPreview)
DECLARE_ID (willBeDeleted)
DECLARE_ID (currentPresetIndex)
DECLARE_ID (latestLoadedPresetIndex)
DECLARE_ID (noteLayoutType)
DECLARE_ID (username)
DECLARE_ID (license)
DECLARE_ID (format)
DECLARE_ID (filesize)
DECLARE_ID (duration)
DECLARE_ID (onsets)
DECLARE_ID (onset)
DECLARE_ID (onsetTime)
DECLARE_ID (midiRootNote)
DECLARE_ID (midiNotes)
DECLARE_ID (midiVelocityLayer)
DECLARE_ID (midiOutForwardsMidiIn)
DECLARE_ID (allSoundsLoaded)

DECLARE_ID (globalMidiInChannel)
DECLARE_ID (numVoices)
DECLARE_ID (useOriginalFilesPreference)
DECLARE_ID (freesoundOauthAccessToken)

DECLARE_ID (sourceDataLocation)
DECLARE_ID (soundsDownloadLocation)
DECLARE_ID (presetFilesLocation)
DECLARE_ID (tmpFilesLocation)
DECLARE_ID (pluginVersion)

DECLARE_ID (reverbRoomSize)
DECLARE_ID (reverbDamping)
DECLARE_ID (reverbWetLevel)
DECLARE_ID (reverbDryLevel)
DECLARE_ID (reverbWidth)
DECLARE_ID (reverbFreezeMode)

DECLARE_ID (ccNumber)
DECLARE_ID (parameterName)
DECLARE_ID (minRange)
DECLARE_ID (maxRange)

DECLARE_ID (numResultsLastQuery)

// --> Start auto-generated code B
DECLARE_ID (launchMode)
DECLARE_ID (startPosition)
DECLARE_ID (endPosition)
DECLARE_ID (loopStartPosition)
DECLARE_ID (loopEndPosition)
DECLARE_ID (loopXFadeNSamples)
DECLARE_ID (reverse)
DECLARE_ID (noteMappingMode)
DECLARE_ID (numSlices)
DECLARE_ID (playheadPosition)
DECLARE_ID (freezePlayheadSpeed)
DECLARE_ID (filterCutoff)
DECLARE_ID (filterRessonance)
DECLARE_ID (filterKeyboardTracking)
DECLARE_ID (filterAttack)
DECLARE_ID (filterDecay)
DECLARE_ID (filterSustain)
DECLARE_ID (filterRelease)
DECLARE_ID (filterADSR2CutoffAmt)
DECLARE_ID (gain)
DECLARE_ID (attack)
DECLARE_ID (decay)
DECLARE_ID (sustain)
DECLARE_ID (release)
DECLARE_ID (pan)
DECLARE_ID (pitch)
DECLARE_ID (pitchBendRangeUp)
DECLARE_ID (pitchBendRangeDown)
DECLARE_ID (mod2CutoffAmt)
DECLARE_ID (mod2GainAmt)
DECLARE_ID (mod2PitchAmt)
DECLARE_ID (mod2PlayheadPos)
DECLARE_ID (vel2CutoffAmt)
DECLARE_ID (vel2GainAmt)
DECLARE_ID (velSensitivity)
DECLARE_ID (midiChannel)
DECLARE_ID (pitchShift)
DECLARE_ID (timeStretch)
// --> End auto-generated code B

DECLARE_ID (sampleStartPosition)
DECLARE_ID (sampleEndPosition)
DECLARE_ID (sampleLoopStartPosition)
DECLARE_ID (sampleLoopEndPosition)

// Volatile state
DECLARE_ID (isQueryingAndDownloadingSounds)
DECLARE_ID (midiInLastStateReportBlock)
DECLARE_ID (lastMIDICCNumber)
DECLARE_ID (lastMIDINoteNumber)
DECLARE_ID (voiceActivations)
DECLARE_ID (voiceSoundIdxs)
DECLARE_ID (voiceSoundPlayPosition)
DECLARE_ID (audioLevels)

#undef DECLARE_ID
}
