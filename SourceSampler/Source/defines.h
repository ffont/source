/*
  ==============================================================================

    defines.h
    Created: 2 Sep 2020 6:20:58pm
    Author:  Frederic Font Corbera

  ==============================================================================
*/

#pragma once

#if ELK_BUILD
    #define USE_EXTERNAL_HTTP_SERVER 1  // In ELK we don't use the embedded HTTP server but use the external one which runs in a separate process
    #define ENABLE_EMBEDDED_HTTP_SERVER 0
    #define STATE_UPDATE_HZ 0  // In ELK we don't send state updates with a timer, but only send state updates in response to requests from external HTTP server app
    #define ENABLE_OSC_SERVER 1  // In ELK we enable OSC interface as this is the way the external UI controls the plugin
    #define USE_EXTERNAL_HTTP_SERVER_FOR_DOWNLOADS 1  // In ELK, downloads also happen through the external HTTP server
#else
    #define ENABLE_EMBEDDED_HTTP_SERVER 1  // Enable embedded http server
    #define STATE_UPDATE_HZ 15  // Send state updates to the embedded http server
    #if JUCE_DEBUG
        #define ENABLE_OSC_SERVER 1 // In debug enable OSC server for testing purposes
        #define USE_EXTERNAL_HTTP_SERVER 1  // ...and also enable external HTTP server so we can test with the ELK blackboard simulator
        #define USE_EXTERNAL_HTTP_SERVER_FOR_DOWNLOADS 1  // Use the external HTTP server for downloads, as if we were in ELK platform
    #else
        #define ENABLE_OSC_SERVER 0 // Don't enable OSC server for non-ELK builds as we won't use this interface in non-ELK release builds
        #define USE_EXTERNAL_HTTP_SERVER 0  // Don't use external server, we only use the embedded one
        #define USE_EXTERNAL_HTTP_SERVER_FOR_DOWNLOADS 0  // Also don't use external server for downloads, we use built-in download functionality
    #endif
#endif


#define ENABLE_DEBUG_BUFFER 0

#define SEND_VOLATILE_STATE_OVER_OSC 1  // This is an optimization for faster volatile state updates when using external HTTP server

#define ELK_SOURCE_DATA_BASE_LOCATION "/udata/source/"
#define ELK_SOURCE_SOUNDS_LOCATION "/udata/source/sounds/"
#define ELK_SOURCE_PRESETS_LOCATION "/udata/source/presets/"
#define ELK_SOURCE_TMP_LOCATION "/udata/source/tmp/"

#define OSC_LISTEN_PORT 9001
#define HTTP_SERVER_LISTEN_PORT 8124
#define HTTP_DOWNLOAD_SERVER_PORT 8123
#define OSC_TO_SEND_PORT 9002  // OSC port where the glue app is listening

#define MAX_SAMPLE_LENGTH 300

#define ACONNECT_MIDI_INTERFACE_ID 16
#define ACONNECT_SUSHI_ID 128

#define FREESOUND_API_REQUEST_TIMEOUT 10000
#define MAX_DOWNLOAD_WAITING_TIME_MS 20000
#define MAX_QUERY_AND_DOWNLOADING_BUSY_TIME 40000

#define LOAD_SAMPLES_IN_THREAD 1
#define MAKE_QUERY_IN_THREAD 1

#define MAX_SIZE_FOR_ORIGINAL_FILE_DOWNLOAD 1024 * 1024 * 15  // 15 MB


// Setters for preset/global parameters
#define ACTION_SET_REVERB_PARAMETERS "/set_reverb_parameters"
#define ACTION_SET_MIDI_IN_CHANNEL "/set_midi_in_channel"
#define ACTION_SET_MIDI_THRU "/set_midi_thru"
#define ACTION_SET_POLYPHONY "/set_polyphony"
#define ACTION_SET_STATE_TIMER_HZ "/set_state_timer_hz"
#define ACTION_SET_USE_ORIGINAL_FILES_PREFERENCE "/set_use_original_files"

// Actions to trigger in plugin
#define ACTION_SAVE_PRESET "/save_preset"
#define ACTION_LOAD_PRESET "/load_preset"
#define ACTION_NEW_QUERY "/new_query"
#define ACTION_REAPPLY_LAYOUT "/reapply_layout"
#define ACTION_CLEAR_ALL_SOUNDS "/clear_all_sounds"
#define ACTION_GET_STATE "/get_state"
#define ACTION_PLAY_SOUND_FROM_PATH "/play_sound_from_path"

// Setters for sound properties (first parameter is soundIndex)
#define ACTION_SET_SOUND_PARAMETER_FLOAT "/set_sound_parameter"
#define ACTION_SET_SOUND_PARAMETER_INT "/set_sound_parameter_int"
#define ACTION_SET_SOUND_SLICES "/set_slices"
#define ACTION_SET_SOUND_ASSIGNED_NOTES "/set_assigned_notes"
#define ACTION_ADD_OR_UPDATE_CC_MAPPING "/add_or_update_cc_mapping"
#define ACTION_REMOVE_CC_MAPPING "/remove_cc_mapping"

// Actions to trigger in plugin for a specific sound (first parameter is soundIndex or soundID)
#define ACTION_ADD_OR_REPLACE_SOUND "/add_or_replace_sound"
#define ACTION_REMOVE_SOUND "/remove_sound"
#define ACTION_PLAY_SOUND "/play_sound"
#define ACTION_STOP_SOUND "/stop_sound"
#define ACTION_SOUND_READY_TO_LOAD "/sound_ready_to_load"
#define ACTION_DOWNLOADING_SOUND_PROGRESS "/downloading_sound_progress"
#define ACTION_FINISHED_DOWNLOADING_SOUND "/finished_downloading_sound"

#define SOUND_TYPE_SINGLE_SAMPLE 0
#define SOUND_TYPE_MULTI_SAMPLE 1

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

#define STATE_FULL_STATE "SourceFullState"
#define STATE_PLUGIN_VERSION "PluginVersion"

#define STATE_PRESET_IDENTIFIER "SourcePresetState"
#define STATE_SAMPLER "Sampler"
#define STATE_PRESET_NAME "presetName"
#define STATE_PRESET_NUMBER "presetNumber"
#define STATE_PRESET_NUMVOICES "NumVoices"
#define STATE_PRESET_NOTE_LAYOUT_TYPE "noteLayoutType"

#define STATE_CURRENT_PORT "currentPort"
#define STATE_LOG_MESSAGES "logMessages"
#define STATE_SOURCE_DATA_LOCATION "sourceDataLocation"
#define STATE_SOUNDS_DATA_LOCATION "soundsDataLocation"
#define STATE_PRESETS_DATA_LOCATION "presetsDataLocation"
#define STATE_TMP_DATA_LOCATION "tmpDataLocation"

#define STATE_SOUNDS_INFO "soundsInfo"
#define STATE_SOUND_INFO "soundInfo"
#define STATE_SOUND_INFO_ID "soundId"
#define STATE_SOUND_INFO_IDX "soundIdx"  // This is the id of the sound in the Sampler object, nothing to do with Freesound
#define STATE_SOUND_INFO_NAME "soundName"
#define STATE_SOUND_INFO_DOWNLOAD_PROGRESS "downloadProgress"
#define STATE_SOUND_INFO_USER "soundUser"
#define STATE_SOUND_INFO_DURATION "soundDurationInSeconds"  // Set when sound loaded in buffer, not before
#define STATE_SOUND_INFO_LICENSE "soundLicense"
#define STATE_SOUND_INFO_OGG_DOWNLOAD_URL "soundOGGURL"
#define STATE_SOUND_INFO_LOCAL_FILE_PATH "soundLocalFilePath"
#define STATE_SOUND_INFO_TYPE "soundType"
#define STATE_SOUND_INFO_SIZE "soundSize"
#define STATE_SOUND_FS_SOUND_ANALYSIS "fsAnalysis"
#define STATE_SOUND_FS_SOUND_ANALYSIS_ONSETS "onsets"
#define STATE_SOUND_FS_SOUND_ANALYSIS_ONSET "onset"
#define STATE_SOUND_FS_SOUND_ANALYSIS_ONSET_TIME "time"
#define STATE_SOUND_INFO_FS_ANALYSIS_ONSET_TIMES "fsAnalysisOnsetTimes"

#define STATE_REVERB_PARAMETERS "ReverbParameters"
#define STATE_REVERB_ROOMSIZE "reverb_roomSize"
#define STATE_REVERB_DAMPING "reverb_damping"
#define STATE_REVERB_WETLEVEL "reverb_wetLevel"
#define STATE_REVERB_DRYLEVEL "reverb_dryLevel"
#define STATE_REVERB_WIDTH "reverb_width"
#define STATE_REVERB_FREEZEMODE "reverb_freezeMode"

#define STATE_SAMPLER_SOUNDS "SamplerSounds"
#define STATE_SAMPLER_NSOUNDS "nSounds"
#define STATE_SAMPLER_SOUND "SamplerSound"
#define STATE_SAMPLER_SOUND_PARAMETER "SamplerSoundParameter"
#define STATE_SAMPLER_SOUND_PARAMETER_TYPE "parameter_type"
#define STATE_SAMPLER_SOUND_PARAMETER_NAME "parameter_name"
#define STATE_SAMPLER_SOUND_PARAMETER_VALUE "parameter_value"
#define STATE_SAMPLER_SOUND_MIDI_NOTES "midiNotes"
#define STATE_SAMPLER_SOUND_LOADED_PREVIEW "loadedPreviewVersion"

#define STATE_SAMPLER_SOUND_MIDI_CC_MAPPING "SamplerSoundMidiCCMapping"
#define STATE_SAMPLER_SOUND_MIDI_CC_MAPPING_RANDOM_ID "midiMappingRandomId"
#define STATE_SAMPLER_SOUND_MIDI_CC_MAPPING_NUMBER "midiMappingCcNumber"
#define STATE_SAMPLER_SOUND_MIDI_CC_MAPPING_NAME "midiMappingParameterName"
#define STATE_SAMPLER_SOUND_MIDI_CC_MAPPING_MIN "midiMappingMinRange"
#define STATE_SAMPLER_SOUND_MIDI_CC_MAPPING_MAX "midiMappingMaxRange"

#define GLOBAL_PERSISTENT_STATE "GlobalSettings"
#define GLOBAL_PERSISTENT_STATE_MIDI_IN_CHANNEL "midiInChannel"
#define GLOBAL_PERSISTENT_STATE_MIDI_THRU "doMidiThru"
#define GLOBAL_PERSISTENT_STATE_PRESET_NUMBER_MAPPING "presetMapping"
#define GLOBAL_PERSISTENT_STATE_PRESET_NUMBER_MAPPING_NUMBER "number"
#define GLOBAL_PERSISTENT_STATE_PRESET_NUMBER_MAPPING_FILENAME "name"
#define GLOBAL_PERSISTENT_STATE_LATEST_LOADED_PRESET "latestPreset"
#define GLOBAL_PERSISTENT_STATE_USE_ORIGINAL_FILES "useOriginalFiles"

#define USE_ORIGINAL_FILES_NEVER "never"
#define USE_ORIGINAL_FILES_ONLY_SHORT "onlyShort"
#define USE_ORIGINAL_FILES_ALWAYS "always"

#define STATE_VOLATILE_IS_QUERYING_AND_DOWNLOADING_SOUNDS "isQueryingAndDownloadingSounds"
#define STATE_VOLATILE_IDENTIFIER "VolatileState"
#define STATE_VOLATILE_VOICE_ACTIVATIONS "voiceActivations"
#define STATE_VOLATILE_VOICE_SOUND_IDXS "voiceSoundIdxs"
#define STATE_VOLATILE_VOICE_SOUND_PLAY_POSITION "voiceSoundPlayPosition"
#define STATE_VOLATILE_AUDIO_LEVELS "audioLevels"
#define STATE_VOLATILE_LAST_MIDI_CC "lastMIDICCNumber"
#define STATE_VOLATILE_LAST_MIDI_NOE "lastMIDINoteNumber"
#define STATE_VOLATILE_MIDI_IN_LAST_STATE_REPORT "midiInLastStateReportBlock"

#define SERIALIZATION_SEPARATOR ";"

namespace Defaults
{
inline int noteLayoutType = NOTE_MAPPING_TYPE_INTERLEAVED;
inline int currentPresetIndex = -1;

// --> Start auto-generated code A
inline int soundType = 0;
inline int launchMode = 0;
inline float  startPosition = 0.0f;
inline float  endPosition = 1.0f;
inline float  loopStartPosition = 0.0f;
inline float  loopEndPosition = 1.0f;
inline int loopXFadeNSamples = 500;
inline int reverse = 0;
inline int noteMappingMode = 0;
inline int numSlices = 0;
inline float  playheadPosition = 0.0f;
inline float  freezePlayheadSpeed = 100.0f;
inline float  filterCutoff = 20000.0f;
inline float  filterRessonance = 0.0f;
inline float  filterKeyboardTracking = 0.0f;
inline float  filterA = 0.01f;
inline float  filterD = 0.0f;
inline float  filterS = 1.0f;
inline float  filterR = 0.01f;
inline float  filterADSR2CutoffAmt = 1.0f;
inline float  gain = -10.0f;
inline float  ampA = 0.01f;
inline float  ampD = 0.0f;
inline float  ampS = 1.0f;
inline float  ampR = 0.01f;
inline float  pan = 0.0f;
inline float  pitch = 0.0f;
inline float  pitchBendRangeUp = 12.0f;
inline float  pitchBendRangeDown = 12.0f;
inline float  mod2CutoffAmt = 10.0f;
inline float  mod2GainAmt = 6.0f;
inline float  mod2PitchAmt = 0.0f;
inline float  mod2PlayheadPos = 0.0f;
inline float  vel2CutoffAmt = 0.0f;
inline float  vel2GainAmt = 0.5f;
// --> End auto-generated code A

inline float reverbRoomSize = 0.5f;
inline float reverbDamping = 0.3f;
inline float reverbWetLevel = 0.0f;
inline float reverbDryLevel = 1.0f;
inline float reverbWidth = 0.5f;
inline float reverbFreezeMode = 0.0f;

inline int randomID = -1;
inline int ccNumber = -1;
inline float minRange = 0.0;
inline float maxRange = 127.0;
}


namespace IDs
{
#define DECLARE_ID(name) const juce::Identifier name (#name);

DECLARE_ID (SOURCE_STATE)
DECLARE_ID (PRESET)
DECLARE_ID (SOUND)
DECLARE_ID (MIDI_CC_MAPPING)
DECLARE_ID (SOUND_SAMPLE)
DECLARE_ID (ANALYSIS)

DECLARE_ID (filePath)
DECLARE_ID (downloadProgress)
DECLARE_ID (downloadCompleted)

DECLARE_ID (uuid)
DECLARE_ID (name)
DECLARE_ID (soundId)
DECLARE_ID (previewURL)
DECLARE_ID (pathInDisk)
DECLARE_ID (enabled)
DECLARE_ID (currentPresetIndex)
DECLARE_ID (noteLayoutType)
DECLARE_ID (username)
DECLARE_ID (license)
DECLARE_ID (format)
DECLARE_ID (filesize)
DECLARE_ID (onsets)
DECLARE_ID (onset)
DECLARE_ID (onsetTime)
DECLARE_ID (midiRootNote)
DECLARE_ID (midiNotes)

DECLARE_ID (reverbRoomSize)
DECLARE_ID (reverbDamping)
DECLARE_ID (reverbWetLevel)
DECLARE_ID (reverbDryLevel)
DECLARE_ID (reverbWidth)
DECLARE_ID (reverbFreezeMode)

DECLARE_ID (randomID)
DECLARE_ID (ccNumber)
DECLARE_ID (parameterName)
DECLARE_ID (minRange)
DECLARE_ID (maxRange)

// --> Start auto-generated code B
DECLARE_ID (soundType)
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
DECLARE_ID (filterA)
DECLARE_ID (filterD)
DECLARE_ID (filterS)
DECLARE_ID (filterR)
DECLARE_ID (filterADSR2CutoffAmt)
DECLARE_ID (gain)
DECLARE_ID (ampA)
DECLARE_ID (ampD)
DECLARE_ID (ampS)
DECLARE_ID (ampR)
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
// --> End auto-generated code B

#undef DECLARE_ID
}
