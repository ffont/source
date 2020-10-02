/*
  ==============================================================================

    defines.h
    Created: 2 Sep 2020 6:20:58pm
    Author:  Frederic Font Corbera

  ==============================================================================
*/

#pragma once

#define ENABLE_HTTP_SERVER 1
#if ELK_BUILD
    #define ENABLE_OSC_SERVER 1
#else
    #define ENABLE_OSC_SERVER 0 // Don't enable OSC server for non-ELK builds as we won't use this interface
#endif
#define ENABLE_DEBUG_BUFFER 0
#define STATE_UPDATE_HZ 15

#define OSC_LISTEN_PORT 9000
#define HTTP_SERVER_LISTEN_PORT 8124
#define HTTP_DOWNLOAD_SERVER_PORT 8123

#define MAX_SAMPLE_LENGTH 300

#define ACTION_NEW_QUERY_TRIGGERED_FROM_SERVER "ACTION_NEW_QUERY_TRIGGERED_FROM_SERVER"
#define ACTION_SET_SOUND_PARAMETER_FLOAT "ACTION_SET_SOUND_PARAMETER_FLOAT"
#define ACTION_SET_SOUND_PARAMETER_INT "ACTION_SET_SOUND_PARAMETER_INT"
#define ACTION_SET_REVERB_PARAMETERS "ACTION_SET_REVERB_PARAMETERS"
#define ACTION_SAVE_CURRENT_PRESET "ACTION_SAVE_CURRENT_PRESET"
#define ACTION_LOAD_PRESET "ACTION_LOAD_PRESET"
#define ACTION_SET_MIDI_IN_CHANNEL "ACTION_SET_MIDI_IN_CHANNEL"
#define ACTION_SET_MIDI_THRU "ACTION_SET_MIDI_THRU"
#define ACTION_PLAY_SOUND "ACTION_PLAY_SOUND"
#define ACTION_STOP_SOUND "ACTION_STOP_SOUND"
#define ACTION_SET_POLYPHONY "ACTION_SET_POLYPHONY"
#define ACTION_FINISHED_DOWNLOADING_SOUND "ACTION_FINISHED_DOWNLOADING_SOUND"
#define ACTION_UPDATE_DOWNLOADING_SOUND_PROGRESS "ACTION_UPDATE_DOWNLOADING_SOUND_PROGRESS"

#define OSC_ADDRESS_NEW_QUERY "/new_query"
#define OSC_ADDRESS_SET_SOUND_PARAMETER_FLOAT "/set_sound_parameter"
#define OSC_ADDRESS_SET_SOUND_PARAMETER_INT "/set_sound_parameter_int"
#define OSC_ADDRESS_SET_REVERB_PARAMETERS "/set_reverb_parameters"
#define OSC_ADDRESS_SAVE_CURRENT_PRESET "/save_current_preset"
#define OSC_ADDRESS_LOAD_PRESET "/load_preset"
#define OSC_ADDRESS_SET_MIDI_IN_CHANNEL "/set_midi_in_channel"
#define OSC_ADDRESS_SET_MIDI_THRU "/set_midi_thru"
#define OSC_ADDRESS_PLAY_SOUND "/play_sound"
#define OSC_ADDRESS_STOP_SOUND "/stop_sound"
#define OSC_ADDRESS_SET_POLYPHONY "/set_polyphony"

#define LAUNCH_MODE_GATE 0
#define LAUNCH_MODE_LOOP 1
#define LAUNCH_MODE_LOOP_FW_BW 2
#define LAUNCH_MODE_TRIGGER 3

#define SLICE_MODE_AUTO_NNOTES 0
#define SLICE_MODE_AUTO_ONSETS 1

#define NOTE_MAPPING_MODE_PITCH 0
#define NOTE_MAPPING_MODE_SLICE 1
#define NOTE_MAPPING_MODE_BOTH 2
#define NOTE_MAPPING_MODE_REPEAT 3

#define STATE_PRESET_IDENTIFIER "SourcePresetState"
#define STATE_SAMPLER "Sampler"
#define STATE_PRESET_NAME "presetName"
#define STATE_PRESET_NUMBER "presetNumber"

#define STATE_CURRENT_PORT "currentPort"
#define STATE_LOG_MESSAGES "logMessages"

#define STATE_QUERY "query"

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
#define STATE_SOUND_FS_SOUND_ANALYSIS "fsAnalysis"
#define STATE_SOUND_FS_SOUND_ANALYSIS_ONSETS "onsets"
#define STATE_SOUND_FS_SOUND_ANALYSIS_ONSET "onset"
#define STATE_SOUND_FS_SOUND_ANALYSIS_ONSET_TIME "time"
#define STATE_SOUND_INFO_FS_ANALYSIS_ONSET_TIMES "fsAnalysisOnsetTimes"

#define STATE_NUMVOICES "NumVoices"

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

#define GLOBAL_PERSISTENT_STATE "GlobalSettings"
#define GLOBAL_PERSISTENT_STATE_MIDI_IN_CHANNEL "midiInChannel"
#define GLOBAL_PERSISTENT_STATE_MIDI_THRU "doMidiThru"
#define GLOBAL_PERSISTENT_STATE_PRESETS_MAPPING "presets"
#define GLOBAL_PERSISTENT_STATE_PRESET_NUMBER_MAPPING "presetMapping"
#define GLOBAL_PERSISTENT_STATE_PRESET_NUMBER_MAPPING_NUMBER "number"
#define GLOBAL_PERSISTENT_STATE_PRESET_NUMBER_MAPPING_FILENAME "name"

#define STATE_VOLATILE_IDENTIFIER "VolatileState"
#define STATE_VOLATILE_VOICE_ACTIVATIONS "voiceActivations"
#define STATE_VOLATILE_VOICE_SOUND_IDXS "voiceSoundIdxs"
#define STATE_VOLATILE_VOICE_SOUND_PLAY_POSITION "voiceSoundPlayPosition"
#define STATE_VOLATILE_AUDIO_LEVELS "audioLevels"

#define SERIALIZATION_SEPARATOR ";"

#define ACONNECT_MIDI_INTERFACE_ID 16
#define ACONNECT_SUSHI_ID 128

#define MAX_DOWNLOAD_WAITING_TIME_MS 20000
#define MAX_QUERY_AND_DOWNLOADING_BUSY_TIME 10000
