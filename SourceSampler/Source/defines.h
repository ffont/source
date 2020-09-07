/*
  ==============================================================================

    defines.h
    Created: 2 Sep 2020 6:20:58pm
    Author:  Frederic Font Corbera

  ==============================================================================
*/

#pragma once

#define ACTION_SHOULD_UPDATE_SOUNDS_TABLE "ACTION_SHOULD_UPDATE_SOUNDS_TABLE"
#define ACTION_NEW_QUERY_TRIGGERED_FROM_SERVER "ACTION_NEW_QUERY_TRIGGERED_FROM_SERVER"
#define ACTION_SET_MIDI_ROOT_NOTE_OFFSET "ACTION_SET_MIDI_ROOT_NOTE_OFFSET"
#define ACTION_SET_SOUND_PARAMETER_FLOAT "ACTION_SET_SOUND_PARAMETER_FLOAT"
#define ACTION_SET_REVERB_PARAMETERS "ACTION_SET_REVERB_PARAMETERS"

#define OSC_ADDRESS_SET_MIDI_ROOT_OFFSET "/set_midi_root_offset"
#define OSC_ADDRESS_NEW_QUERY "/new_query"
#define OSC_ADDRESS_SET_SOUND_PARAMETER_FLOAT "/set_sound_parameter"
#define OSC_ADDRESS_SET_REVERB_PARAMETERS "/set_reverb_parameters"

#define STATE_PRESET_IDENTIFIER "SourcePresetState"
#define STATE_SAMPLER "Sampler"

#define STATE_QUERY "query"

#define STATE_SOUNDS_INFO "soundsInfo"
#define STATE_SOUND_INFO "soundInfo"
#define STATE_SOUND_INFO_ID "soundId"
#define STATE_SOUND_INFO_NAME "soundName"
#define STATE_SOUND_INFO_USER "soundUser"
#define STATE_SOUND_INFO_LICENSE "soundLicense"
#define STATE_SOUND_INFO_OGG_DOWNLOAD_URL "soundOGGURL"

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
#define STATE_SAMPLER_SOUND_PARAMETERS "SamplerSoundParameters"
#define STATE_SAMPLER_SOUND_PARAMETER "SamplerSoundParameter"
#define STATE_SAMPLER_SOUND_PARAMETER_TYPE "parameter_type"
#define STATE_SAMPLER_SOUND_PARAMETER_NAME "parameter_name"
#define STATE_SAMPLER_SOUND_PARAMETER_VALUE "parameter_value"



#define SERIALIZATION_SEPARATOR ";"

#define OSC_LISTEN_PORT 9000

#define ACONNECT_MIDI_INTERFACE_ID 16
#define ACONNECT_SUSHI_ID 128

#define MAX_DOWNLOAD_WAITING_TIME_MS 20000
