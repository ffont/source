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

#define SERIALIZATION_SEPARATOR ";"

#define OSC_LISTEN_PORT 9000
