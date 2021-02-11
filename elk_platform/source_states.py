import json
import math
import os
import random
import time
import traceback

import numpy
from scipy.io import wavfile
import pyogg
import aifc

from freesound_api_key import FREESOUND_CLIENT_ID
from freesound_interface import find_sound_by_similarity, find_sound_by_query, find_sounds_by_query, find_random_sounds, logout_from_freesound, \
    is_logged_in, get_crurrently_logged_in_user, get_user_bookmarks, bookmark_sound
from helpers import justify_text, frame_from_lines, frame_from_start_animation, add_global_message_to_frame, START_ANIMATION_DURATION, \
    translate_cc_license_url, StateNames, add_scroll_bar_to_frame, add_centered_value_to_frame, add_sound_waveform_and_extras_to_frame, \
    DISPLAY_SIZE, add_midi_keyboard_and_extras_to_frame, add_text_input_to_frame, merge_dicts, raw_assigned_notes_to_midi_assigned_notes, \
    add_to_sp_cache, get_sp_parameter_value_from_cache, add_recent_query, add_recent_query_filter, get_recent_queries, \
    get_recent_query_filters, add_meter_to_frame, add_voice_grid_to_frame, get_filenames_in_dir, clear_moving_text_cache, sizeof_fmt

try:
    from elk_ui_custom import N_LEDS, N_FADERS
except ModuleNotFoundError:
    N_LEDS = 9

ALLOWED_AUDIO_FILE_EXTENSIONS = ['ogg', 'wav', 'aiff', 'mp3', 'flac']
ALLOWED_AUDIO_FILE_EXTENSIONS_IN_SLICE_EDITOR = ['ogg', 'wav', 'aiff']

def get_local_audio_files_path():
    if sm is not None and sm.source_state:
        base_path = sm.source_state.get(StateNames.SOURCE_DATA_LOCATION, None)
        if base_path is not None:
            base_path = os.path.join(base_path, 'local_files')
            if not os.path.exists(base_path):
                os.makedirs(base_path)
            return base_path
    return None


def snap_to_value(x, value=0.5, margin=0.07):
    if abs(x - value) < margin:
        return value
    return x

def lin_to_exp(x):
    sign = 1 if x >= 0 else -1
    return sign * pow(x, 2)

#  send_func (norm to val), get_func (val to val), parameter_label, value_label_template, set osc address
sound_parameters_info_dict = {
    "gain": (lambda x: 12.0 * lin_to_exp(2.0 * (snap_to_value(x) - 0.5)) if x >= 0.5 else 36.0 * lin_to_exp(2.0 * (snap_to_value(x) - 0.5)), lambda x: float(x), "Gain", "{0:.2f}dB", "/set_sound_parameter"),
    "pitch": (lambda x: 36.0 * lin_to_exp(2.0 * (snap_to_value(x) - 0.5)), lambda x: float(x), "Pitch", "{0:.2f}", "/set_sound_parameter"),
    "reverse": (lambda x: int(round(x)), lambda x: ['Off', 'On'][int(x)], "Reverse", "{0}", "/set_sound_parameter_int"),
    "launchMode": (lambda x: int(round(4 * x)), lambda x: ['Gate', 'Loop', 'Ping-pong', 'Trigger', 'Freeze'][int(x)], "Launch mode", "{0}", "/set_sound_parameter_int"),
    "startPosition": (lambda x: x, lambda x: float(x), "Start pos", "{0:.4f}", "/set_sound_parameter"),
    "endPosition": (lambda x: x, lambda x: float(x), "End pos", "{0:.4f}", "/set_sound_parameter"),
    "loopStartPosition": (lambda x: x, lambda x: float(x), "Loop st pos", "{0:.4f}", "/set_sound_parameter"),
    "loopEndPosition": (lambda x: x, lambda x: float(x), "Loop end pos", " {0:.4f}", "/set_sound_parameter"),
    "ampADSR.attack": (lambda x: 20.0 * lin_to_exp(x), lambda x: float(x), "A", "{0:.2f}s", "/set_sound_parameter"),
    "ampADSR.decay": (lambda x: 20.0 * lin_to_exp(x), lambda x: float(x), "D", "{0:.2f}s", "/set_sound_parameter"),
    "ampADSR.sustain": (lambda x: x, lambda x: float(x), "S", "{0:.2f}", "/set_sound_parameter"),
    "ampADSR.release": (lambda x: 20.0 * lin_to_exp(x), lambda x: float(x), "R", "{0:.2f}s", "/set_sound_parameter"),
    "filterCutoff": (lambda x: 10 + 20000 * lin_to_exp(x), lambda x: float(x), "Cutoff", "{0:.2f}Hz", "/set_sound_parameter"),
    "filterRessonance": (lambda x: x, lambda x: float(x), "Resso", "{0:.2f}", "/set_sound_parameter"),
    "filterKeyboardTracking": (lambda x: x, lambda x: float(x), "K.T.", "{0:.2f}", "/set_sound_parameter"),
    "filterADSR2CutoffAmt": (lambda x: 100.0 * x, lambda x: float(x), "Env amt", "{0:.0f}%", "/set_sound_parameter"),    
    "filterADSR.attack": (lambda x: 20.0 * lin_to_exp(x), lambda x: float(x), "Filter A", "{0:.2f}s", "/set_sound_parameter"),
    "filterADSR.decay": (lambda x: 20.0 * lin_to_exp(x), lambda x: float(x), "Filter D", "{0:.2f}s", "/set_sound_parameter"),
    "filterADSR.sustain": (lambda x: x, lambda x: float(x), "Filter S", "{0:.2f}", "/set_sound_parameter"),
    "filterADSR.release": (lambda x: 20.0 * lin_to_exp(x), lambda x: float(x), "Filter R", "{0:.2f}s", "/set_sound_parameter"),
    "noteMappingMode": (lambda x: int(round(3 * x)), lambda x: ['Pitch', 'Slice', 'Both', 'Repeat'][int(x)], "Map mode", "{0}", "/set_sound_parameter_int"),
    "numSlices": (lambda x: int(round(32.0 * x)), lambda x: (['Auto onsets', 'Auto notes']+[str(x) for x in range(2, 101)])[int(x)], "# slices", "{0}", "/set_sound_parameter_int"),
    "playheadPosition": (lambda x: x, lambda x: float(x), "Playhead", "{0:.4f}", "/set_sound_parameter"),
    "freezePlayheadSpeed": (lambda x: 1 + 4999 * lin_to_exp(x), lambda x: float(x), "Freeze speed", "{0:.1f}", "/set_sound_parameter"),
    "pitchBendRangeUp": (lambda x: 36.0 * x, lambda x: float(x), "P.Bend down", "{0:.1f}st", "/set_sound_parameter"),
    "pitchBendRangeDown": (lambda x: 36.0 * x, lambda x: float(x), "P.Bend up", "{0:.1f}st", "/set_sound_parameter"),
    "mod2CutoffAmt": (lambda x: 100.0 * x, lambda x: float(x), "Mod2Cutoff", "{0:.0f}%", "/set_sound_parameter"),
    "mod2GainAmt": (lambda x: 12.0 * lin_to_exp(2.0 * (snap_to_value(x) - 0.5)), lambda x: float(x), "Mod2Gain", "{0:.1f}dB", "/set_sound_parameter"),
    "mod2PitchAmt": (lambda x: 12.0 * lin_to_exp(2.0 * (snap_to_value(x) - 0.5)), lambda x: float(x), "Mod2Pitch", "{0:.2f}st", "/set_sound_parameter"),
    "vel2CutoffAmt": (lambda x: 100.0 * x, lambda x: float(x), "Vel2Cutoff", "{0:.0f}%", "/set_sound_parameter"),
    "vel2GainAmt": (lambda x: x, lambda x: int(100 * float(x)), "Vel2Gain", "{0}%", "/set_sound_parameter"),
    "pan": (lambda x: 2.0 * (snap_to_value(x) - 0.5), lambda x: float(x), "Panning", "{0:.1f}", "/set_sound_parameter"),
    "midiRootNote": (lambda x: int(round(x * 127)), lambda x: int(x), "Root note", "{0}", "/set_sound_parameter_int"),
}

EXTRA_PAGE_1_NAME = "extra1"
EXTRA_PAGE_2_NAME = "extra2"
sound_parameter_pages = [
    [
        "gain",
        "pitch",
        "reverse",
        "launchMode",
    ], [
        "startPosition",
        "endPosition",
        "loopStartPosition",
        "loopEndPosition",
    ], [
        "ampADSR.attack",
        "ampADSR.decay",
        "ampADSR.sustain",
        "ampADSR.release",
    ], [
        "filterCutoff",
        "filterRessonance",
        "filterKeyboardTracking",
        "filterADSR2CutoffAmt",
    ], [
        "filterADSR.attack",
        "filterADSR.decay",
        "filterADSR.sustain",
        "filterADSR.release",
    ], [
        "noteMappingMode",
        "numSlices",
        "playheadPosition",
        "freezePlayheadSpeed"
    ], [
        "pitchBendRangeUp",
        "pitchBendRangeDown",
        "vel2CutoffAmt",
        "vel2GainAmt"
    ], [
        "mod2CutoffAmt",
        "mod2GainAmt",
        "mod2PitchAmt",
        "pan"
    ]
]

reverb_parameters_pages = [
    [
        "roomSize", 
        "damping", 
        "width", 
        "freezeMode"
    ], [
        "wetLevel", 
        "dryLevel",
        None,
        None
    ]
]

#  parameter position in OSC message, parameter label
reverb_parameters_info_dict = {
    "roomSize": (0, "Room size"), 
    "damping": (1, "Damping"), 
    "wetLevel": (2, "Wet level"), 
    "dryLevel": (3, "Dry level"),  
    "width": (4, "Width"), 
    "freezeMode": (5, "Freeze"), 
}

note_layout_types = ['Contiguous', 'Interleaved']
license_types = ['All', 'CC0', 'Exclude NC']
num_sounds_options = [1, 2, 3, 4, 5, 6, 8, 12, 16, 24, 32, 64]
ac_descriptors_options = ['off', 'low', 'mid', 'high']
ac_descriptors_names = ['brightness', 'hardness', 'depth', 'roughness','boominess', 'warmth', 'sharpness']

query_settings_info_dict = {
    'num_sounds': (lambda x:num_sounds_options[int(round(x * (len(num_sounds_options) - 1)))], 'Num sounds', '{}', 16),
    'min_length': (lambda x:float(pow(x, 4) * 300), 'Min length', '{0:.1f}s', 0.0),
    'max_length': (lambda x:float(pow(x, 4) * 300), 'Max length', '{0:.1f}s', 10.0),
    'layout': (lambda x:note_layout_types[int(round(x * (len(note_layout_types) - 1)))], 'Layout', '{}', note_layout_types[1]),
    'brightness': (lambda x:ac_descriptors_options[int(round(x * (len(ac_descriptors_options) - 1)))], 'Brightness', '{}', ac_descriptors_options[0]),
    'hardness': (lambda x:ac_descriptors_options[int(round(x * (len(ac_descriptors_options) - 1)))], 'Hardness', '{}', ac_descriptors_options[0]),
    'depth': (lambda x:ac_descriptors_options[int(round(x * (len(ac_descriptors_options) - 1)))], 'Depth', '{}', ac_descriptors_options[0]),
    'roughness': (lambda x:ac_descriptors_options[int(round(x * (len(ac_descriptors_options) - 1)))], 'Roughness', '{}', ac_descriptors_options[0]),
    'boominess': (lambda x:ac_descriptors_options[int(round(x * (len(ac_descriptors_options) - 1)))], 'Boominess', '{}', ac_descriptors_options[0]),
    'warmth': (lambda x:ac_descriptors_options[int(round(x * (len(ac_descriptors_options) - 1)))], 'Warmth', '{}', ac_descriptors_options[0]),
    'sharpness': (lambda x:ac_descriptors_options[int(round(x * (len(ac_descriptors_options) - 1)))], 'Sharpness', '{}', ac_descriptors_options[0]),
    'license':  (lambda x:license_types[int(round(x * (len(license_types) - 1)))], 'License', '{}', license_types[0]),
}

query_settings_pages = [
    [
        'num_sounds',
        'min_length',
        'max_length',
        'layout'
    ], [
        'brightness',
        'hardness',
        'depth',
        'roughness'
    ], [
        'boominess',
        'warmth',
        'sharpness',
        'license'
    ]
]

midi_cc_available_parameters_list = ["startPosition", "endPosition", "loopStartPosition", "loopEndPosition", "playheadPosition", "freezePlayheadSpeed", "filterCutoff", "filterRessonance", "gain", "pan", "pitch"]

predefined_queries = ['wind', 'rain', 'piano', 'explosion', 'music', 'whoosh', 'woosh', 'intro', 'birds', 'footsteps', 'fire', '808', 'scream', 'water', 'bell', 'click', 'thunder', 'guitar', 'bass', 'beep', 'swoosh', 'pop', 'cartoon', 'magic', 'car', 'horror', 'vocal', 'game', 'trap', 'lofi', 'clap', 'happy', 'forest', 'ding', 'drum', 'kick', 'glitch', 'drop', 'transition', 'animal', 'gun', 'door', 'hit', 'punch', 'nature', 'jump', 'flute', 'sad', 'beat', 'christmas']
predefined_queries = sorted(predefined_queries)


class StateManager(object):

    state_stack = []
    global_message = ('', 0, 0)  # (text, starttime, duration)
    osc_client = None
    ui_client = None
    source_state = {}
    frame_counter = 0
    open_url_in_browser = None
    should_show_start_animation = True
    block_ui_input = False
    waiting_to_go_to_last_loaded_sound = False

    def set_osc_client(self, osc_client):
        self.osc_client = osc_client

    def set_ui_client(self, ui_client):
        self.ui_client = ui_client

    def update_source_state(self, source_state):
        self.source_state = source_state
        self.current_state.on_source_state_update()

        if self.waiting_to_go_to_last_loaded_sound and self.source_state.get(StateNames.NUM_SOUNDS_CHANGED, False):
            self.move_to(SoundSelectedState(sound_idx=999))  # If num sounds have changed and we were waiting for this change, move to last sound
            self.waiting_to_go_to_last_loaded_sound = False

    def set_waiting_to_go_to_last_loaded_sound(self):
        self.waiting_to_go_to_last_loaded_sound = True

    def set_led(self, led_idx, unset_others=False):
        if self.ui_client is not None:
            if unset_others:
                for led_idx2 in range(0, N_LEDS):
                    if led_idx2 != led_idx:
                        self.ui_client.set_led(led_idx2, 0)
            self.ui_client.set_led(led_idx, 1)

    def unset_led(self, led_idx):
        if self.ui_client is not None:
            self.ui_client.set_led(led_idx, 0)

    def unset_all_leds(self):
        if self.ui_client is not None:
            for led_idx in range(0, N_LEDS):
                self.ui_client.set_led(led_idx, 0)

    def set_fader_led(self, fader_idx, unset_others=False):
        if self.ui_client is not None:
            if unset_others:
                for fader_idx2 in range(0, N_FADERS):
                    if fader_idx2 != fader_idx:
                        self.ui_client.set_fader_led(fader_idx2, 0)
            self.ui_client.set_fader_led(fader_idx, 1)

    def unset_all_fader_leds(self):
        if self.ui_client is not None:
            for fader_idx in range(0, N_FADERS):
                self.ui_client.set_fader_led(fader_idx, 0)

    def set_all_fader_leds(self):
        if self.ui_client is not None:
            for fader_idx in range(0, N_FADERS):
                self.ui_client.set_fader_led(fader_idx, 1)

    def send_osc_to_plugin(self, address, values):
        if self.osc_client is not None:
            if address in ['/set_sound_parameter', '/set_sound_parameter_int']:
                # If we're setting a parameter, cache its value so we can use the cached value to display in UI
                # before the plugin reports an updated state
                add_to_sp_cache(*values)
            self.osc_client.send_message(address, values)

    def draw_display_frame(self):
        self.frame_counter += 1

        # Compute display frame of the current ui state and plugin state variables
        if not self.current_state.should_show_help():
            frame = self.current_state.draw_display_frame()
        else:
            frame = self.current_state.get_help_page_frame()

        # If a global message should be added, do it here
        if self.global_message[0] != '':
            if time.time() - self.global_message[1] < self.global_message[2]:
                add_global_message_to_frame(frame, self.global_message[0])
            else:
                self.global_message = ('', 0, 0)

        return frame

    def show_global_message(self, text, duration=1):
        self.global_message = (text, time.time(), duration)

    def move_to(self, new_state, replace_current=False):
        clear_moving_text_cache()
        if self.state_stack:
            self.current_state.exit_help()
        if replace_current:
            self.current_state.on_deactivating_state()
            self.state_stack.pop()
        self.state_stack.append(new_state)
        self.current_state.on_activating_state()

    def go_back(self, n_times=1):
        for i in range(0, n_times):
            if len(self.state_stack) > 1:
                self.current_state.on_deactivating_state()
                self.state_stack.pop()
                self.current_state.on_activating_state()

    def get_source_state_selected_sound_property(self, sound_idx, property_name, default="-"):
        if self.source_state is not None and StateNames.SOUNDS_INFO in self.source_state:
            if sound_idx < self.source_state.get(StateNames.NUM_SOUNDS, 0):
                return self.source_state[StateNames.SOUNDS_INFO][sound_idx][property_name]
        return default

    def gsp(self, sound_idx, property_name, default=None):
        # Shorthand for get_source_state_selected_sound_property
        return self.get_source_state_selected_sound_property(sound_idx, property_name, default=default)

    def get_source_state_selected_sound_sound_parameter(self, sound_idx, parameter_name, default="-"):
        sound_parameters = self.gsp(sound_idx, StateNames.SOUND_PARAMETERS, default={})
        if sound_parameters:
            # Check if there's a cached version of the value, otherwise return the one on state
            cached_val = get_sp_parameter_value_from_cache(sound_idx, parameter_name)
            if cached_val is not None:
                return cached_val
            else:
                return sound_parameters.get(parameter_name, default)
        return default

    def gsparam(self, sound_idx, parameter_name, default="-"):
        return self.get_source_state_selected_sound_sound_parameter(sound_idx, parameter_name, default=default)

    def is_waiting_for_data_from_web(self):
        return type(self.current_state) == EnterDataViaWebInterfaceState or type(self.current_state) == EnterTextViaHWOrWebInterfaceState

    def process_data_from_web(self, data):
        if self.is_waiting_for_data_from_web():
            self.current_state.on_data_received(data)

    @property
    def current_state(self):
        return self.state_stack[-1]


class State(object):

    name = ""
    activation_time = None
    current_help_page = None
    help_pages = []
    help_page_title = "Help"
    trigger_help_page_button_id = 8

    def __init__(self, *args, **kwargs):
        self.activation_time = time.time()
        self.name = self.__class__.__name__

    def __str__(self):
        serialized = 'STATE: {0}'.format(self.name)
        for key, value in self.get_properties().items():
            serialized += '\n- {0}: {1}'.format(key, value)
        return serialized

    def get_properties(self):
        return {}

    @property
    def seconds_since_activation(self):
        return time.time() - self.activation_time

    def exit_help(self):
        self.current_help_page = None

    def should_show_help(self):
        return self.current_help_page is not None

    def get_help_page_frame(self):
        lines = [{
                "underline": True, 
                "text": self.help_page_title
                }
            ]
        if self.current_help_page != None:  
            lines += self.help_pages[self.current_help_page]
        return frame_from_lines(lines) 

    def draw_display_frame(self):
        lines = [
            self.get_default_header_line(),
            {"underline": True, 
            "text": self.name},
            str(self.get_properties())
        ]
        return frame_from_lines(lines)

    def get_default_header_line(self):
        indicators = "{}{}{}{}".format(
            "M" if sm.source_state.get(StateNames.MIDI_RECEIVED, False) else "",
            "!" if not sm.source_state[StateNames.NETWORK_IS_CONNECTED] else "", 
            "W" if sm.source_state.get(StateNames.IS_QUERYING_AND_DOWNLOADING, False) else "", 
            #"*" if sm.source_state[StateNames.STATE_UPDATED_RECENTLY] else "", 
            ["|", "/", "-", "\\"][sm.frame_counter % 4]
        )
        return {
            "invert": True, 
            "text": justify_text("{0}:{1}".format(
                sm.source_state.get(StateNames.LOADED_PRESET_INDEX, -1), 
                sm.source_state.get(StateNames.LOADED_PRESET_NAME, 'NoName')), 
                indicators)
        }

    def get_sound_idx_from_buttons(self, button_idx, shift=False):
        sound_idx = -1
        if button_idx > 0:
            sound_idx = button_idx - 1   # from 0-7
            if shift:  # if "shift" button is pressed, sound index is form 8-15
                sound_idx += 8
    
            num_sounds = sm.source_state.get(StateNames.NUM_SOUNDS, 0)
            if sound_idx < num_sounds:
                return sound_idx
            else:
                return num_sounds
        return sound_idx

    def save_current_preset(self):
        current_preset_name = sm.source_state.get(StateNames.LOADED_PRESET_NAME, 'NoName')
        current_preset_index = sm.source_state.get(StateNames.LOADED_PRESET_INDEX, -1)
        if current_preset_index > -1:
            sm.send_osc_to_plugin("/save_current_preset", [current_preset_name, int(current_preset_index)])
            sm.show_global_message("Saving {}\n{}...".format(current_preset_index, current_preset_name))

    def save_current_preset_to(self, query="", preset_idx=-1):
        preset_name = query  # NOTE: the parameter is called "query" because it reuses some classes used for entering queries. We might want to change that to "name"
        sm.send_osc_to_plugin("/save_current_preset", [preset_name, int(preset_idx)])
        sm.show_global_message("Saving {}\n{}...".format(preset_idx, preset_name))

    def load_preset(self, preset_idx):
        preset_name = ""
        if ':' in preset_idx:
            preset_name = preset_idx.split(':')[1]
            preset_idx = int(preset_idx.split(':')[0])  # This is because we're passing preset_idx:preset_name format (some callbacks might use it)
        sm.send_osc_to_plugin("/load_preset", [int(preset_idx)])
        if preset_name:
            sm.show_global_message("Loading {}\n{}...".format(preset_idx, preset_name), duration=5)
        else:
            sm.show_global_message("Loading {}...".format(preset_idx), duration=5)

    def reload_current_preset(self):
        current_preset_index = sm.source_state.get(StateNames.LOADED_PRESET_INDEX, -1)
        current_preset_name = sm.source_state.get(StateNames.LOADED_PRESET_NAME, "unnamed")
        if current_preset_index > -1:
            sm.send_osc_to_plugin("/load_preset", [int(current_preset_index)])
            sm.show_global_message("Loading {}\n{}...".format(current_preset_index, current_preset_name), duration=5)

    def reapply_note_layout(self, layout_type):
        sm.send_osc_to_plugin("/reapply_layout", [['Contiguous', 'Interleaved'].index(layout_type)]) 
        sm.show_global_message("Updated layout")

    def set_num_voices(self, num_voices):
        sm.send_osc_to_plugin("/set_polyphony", [num_voices])

    def set_download_original_files(self, preference):
        sm.send_osc_to_plugin("/set_use_original_files", [{
            'Never': 'never',
            'Only small': 'onlyShort',
            'Always': 'always',
        }[preference]])

    def send_add_or_replace_sound_to_plugin(self, sound_idx, new_sound, assinged_notes="", root_note=-1, trigger_download="", local_file_path="", move_once_loaded=False):
        sound_onsets_list = []
        if 'analysis' in new_sound and new_sound['analysis'] is not None:
            if 'rhythm' in new_sound['analysis']:
                if 'onset_times' in new_sound['analysis']['rhythm']:
                    sound_onsets_list = new_sound['analysis']['rhythm']['onset_times']
                    if type(sound_onsets_list) != list:
                        # This can happen because when there's only one onset, FS api returns it as a number instead of a list of one element
                        sound_onsets_list = [sound_onsets_list]
        # Don't send more than 128 onsets because these won't be mapped anyway
        # Sending too many onsets might render the UPD OSC message too long
        sound_onsets_list = sound_onsets_list[:128]  

        sm.send_osc_to_plugin("/add_or_replace_sound", [
            sound_idx, 
            new_sound['id'], 
            new_sound['name'], 
            new_sound['username'], 
            new_sound['license'], 
            new_sound['previews']['preview-hq-ogg'],
            local_file_path,
            new_sound['type'],
            new_sound['filesize'],
            ','.join(str(o) for o in sound_onsets_list),  # for use as slices
            assinged_notes,  # This should be string representation of hex number
            root_note, # The note to be used as root
            trigger_download  # "" -> yes, "none" -> no, "all" -> all sounds
            ])

        if sound_idx == -1 and move_once_loaded:
            sm.set_waiting_to_go_to_last_loaded_sound()

    def load_query_results(self, new_sounds, note_mapping_type=1, assigned_notes_per_sound_list=[]):
        sm.send_osc_to_plugin('/clear_all_sounds', [])
        time.sleep(0.2)
        n_sounds = len(new_sounds)
        n_notes_per_sound = 128 // n_sounds
        for sound_idx, sound in enumerate(new_sounds):
            if len(assigned_notes_per_sound_list) < len(new_sounds):
                # If assigned_notes_per_sound_list is not provided or its length is shorter than the sounds we're loading,
                # make new note layout following note_mapping_type
                midi_notes = ["0"] * 128
                root_note = None
                if note_layout_types[note_mapping_type] == 'Contiguous':
                    # In this case, all the notes mapped to this sound are contiguous in a range which depends on the total number of sounds to load
                    root_note = sound_idx * n_notes_per_sound + n_notes_per_sound // 2
                    for i in range(sound_idx * n_notes_per_sound, (sound_idx + 1) * n_notes_per_sound):
                        midi_notes[i] = "1"
                else:
                    # Notes are mapped to sounds in interleaved fashion so each contiguous note corresponds to a different sound.
                    NOTE_MAPPING_INTERLEAVED_ROOT_NOTE = 36
                    root_note = NOTE_MAPPING_INTERLEAVED_ROOT_NOTE + sound_idx
                    for i in range(root_note, 128, n_sounds):
                        midi_notes[i] = "1"  # Map notes in upwards direction
                    for i in range(root_note, 0, -n_sounds):
                        midi_notes[i] = "1"  # Map notes in downwards direction
                
                midi_notes = hex(int("".join(reversed(midi_notes)), 2))  # Convert to a format expected by send_add_or_replace_sound_to_plugin
            else:
                # If assigned_notes_per_sound_list is provided, take midi notes info from there
                midi_notes = assigned_notes_per_sound_list[sound_idx]
                root_note = assigned_notes_per_sound_list[len(assigned_notes_per_sound_list)//2]

            self.send_add_or_replace_sound_to_plugin(-1, sound, assinged_notes=midi_notes, root_note=root_note, trigger_download="none" if sound_idx < n_sounds -1 else "all")

    def consolidate_ac_descriptors_from_kwargs(self, kwargs_dict):
        return {key: value for key, value in kwargs_dict.items() if key in ac_descriptors_names and value != 'off'}


    def add_or_replace_sound_by_query(self, sound_idx=-1, query='', min_length=0, max_length=300, page_size=50, move_once_loaded=False, **kwargs):
        sm.show_global_message("Searching\n{}...".format(query), duration=3600)
        sm.block_ui_input = True
        try:
            selected_sound = find_sound_by_query(query=query, min_length=min_length, max_length=max_length, page_size=page_size, license=kwargs.get('license', None), ac_descriptors_filters=self.consolidate_ac_descriptors_from_kwargs(kwargs))
            if selected_sound is not None:
                sm.show_global_message("Loading sound...")
                self.send_add_or_replace_sound_to_plugin(sound_idx, selected_sound, move_once_loaded=move_once_loaded)
            else:
                sm.show_global_message("No results found!")
        except Exception as e:
            print("ERROR while querying Freesound: {0}".format(e))
            traceback.print_tb(e.__traceback__)
            sm.show_global_message("Error :(")
        sm.block_ui_input = False

    def replace_sound_by_similarity(self, sound_idx, selected_sound_id):
        sm.block_ui_input = True
        sm.show_global_message("Searching\nsimilar...", duration=3600)
        try:
            selected_sound = find_sound_by_similarity(selected_sound_id)
            if selected_sound is not None:
                sm.show_global_message("Replacing sound...")
                self.send_add_or_replace_sound_to_plugin(sound_idx, selected_sound)
            else:
                sm.show_global_message("No results found!")
        except Exception as e:
            print("ERROR while querying Freesound: {0}".format(e))
            traceback.print_tb(e.__traceback__)
            sm.show_global_message("Error :(")
        sm.block_ui_input = False

    def add_or_replace_sound_random(self, sound_idx=-1, num_sounds=1, min_length=0, max_length=10, layout=1, move_once_loaded=False, **kwargs):
        sm.show_global_message("Finding\nrandom...", duration=3600)
        sm.block_ui_input = True
        try:
            new_sound = find_random_sounds(n_sounds=1, min_length=min_length, max_length=max_length, license=kwargs.get('license', None), ac_descriptors_filters=self.consolidate_ac_descriptors_from_kwargs(kwargs))
            if not new_sound:
                sm.show_global_message("No sound found!")
            else:
                new_sound = new_sound[0]
                sm.show_global_message("Loading sound...")
                self.send_add_or_replace_sound_to_plugin(sound_idx, new_sound, move_once_loaded=move_once_loaded)
        except Exception as e:
            print("ERROR while querying Freesound: {0}".format(e))
            traceback.print_tb(e.__traceback__)
            sm.show_global_message("Error :(")
        sm.block_ui_input = False

    def new_preset_by_query(self, query='', num_sounds=16, min_length=0, max_length=300, layout=1, page_size=150, **kwargs):
        sm.show_global_message("Searching\n{}...".format(query), duration=3600)
        sm.block_ui_input = True
        try:
            new_sounds = find_sounds_by_query(query=query, n_sounds=num_sounds, min_length=min_length, max_length=max_length, page_size=page_size, license=kwargs.get('license', None), ac_descriptors_filters=self.consolidate_ac_descriptors_from_kwargs(kwargs))
            if not new_sounds:
                sm.show_global_message("No results found!")
            else:
                sm.show_global_message("Loading sounds...")
                self.load_query_results(new_sounds, layout)
                
        except Exception as e:
            print("ERROR while querying Freesound: {0}".format(e))
            traceback.print_tb(e.__traceback__)
            sm.show_global_message("Error :(")
        sm.block_ui_input = False

    def new_preset_from_predefined_query(self, query, **kwargs):
        kwargs.update({"query": query})
        self.new_preset_by_query(**kwargs)  

    def new_preset_by_random_sounds(self, num_sounds=16, min_length=0, max_length=10, layout=1, **kwargs):

        def show_progress_message(count, total):
            sm.show_global_message("Entering the\nunknown... [{}/{}]".format(count + 1, total), duration=3600)
        
        sm.show_global_message("Entering the\nunknown...", duration=3600)
        sm.block_ui_input = True
        try:
            new_sounds = find_random_sounds(n_sounds=num_sounds, min_length=min_length, max_length=max_length, report_callback=show_progress_message, license=kwargs.get('license', None), ac_descriptors_filters=self.consolidate_ac_descriptors_from_kwargs(kwargs))
            if not new_sounds:
                sm.show_global_message("No results found!")
            else:
                sm.show_global_message("Loading sounds...")
                self.load_query_results(new_sounds, layout)

        except Exception as e:
            print("ERROR while querying Freesound: {0}".format(e))
            traceback.print_tb(e.__traceback__)
            sm.show_global_message("Error :(")
        sm.block_ui_input = False

    def new_preset_by_similar_sounds(self, note_mapping_type=1):

        def show_progress_message(count, total):
            sm.show_global_message("Searching similar\nsounds... [{}/{}]".format(count + 1, total), duration=3600)

        sm.show_global_message("Searching similar\nsounds...", duration=3600)
        sm.block_ui_input = True
        try:
            new_sounds = []
            existing_sounds = sm.source_state.get(StateNames.SOUNDS_INFO, [])
            existing_sounds_assigned_notes = []
            for count, sound in enumerate(existing_sounds):
                existing_sounds_assigned_notes.append(sound[StateNames.SOUND_ASSIGNED_NOTES])
                sound_id = sound[StateNames.SOUND_ID]
                if sound_id != 0:
                    new_sound = find_sound_by_similarity(sound_id)
                    if new_sound is not None:
                        new_sounds.append(new_sound)
                    else:
                        new_sounds.append(existing_sounds[count])  # If no similar sound is found, use the current one
                    show_progress_message(count + 1, len(existing_sounds))

            sm.show_global_message("Loading sounds...")
            self.load_query_results(new_sounds, note_mapping_type=note_mapping_type, assigned_notes_per_sound_list=existing_sounds_assigned_notes)

        except Exception as e:
            print("ERROR while querying Freesound: {0}".format(e))
            traceback.print_tb(e.__traceback__)
            sm.show_global_message("Error :(")
        sm.block_ui_input = False

    def set_slices_for_sound(self, sound_idx, slices):
        if sound_idx >  -1:
            sm.send_osc_to_plugin('/set_slices', [sound_idx, ','.join([str(s) for s in slices])])

    def get_sound_idx_from_note(self, midi_note):
        # Iterate over sounds to find the first one that has that note assigned
        for i, sound in enumerate(sm.source_state.get(StateNames.SOUNDS_INFO, [])):
            raw_assigned_notes = sound.get(StateNames.SOUND_ASSIGNED_NOTES, None)
            if raw_assigned_notes is not None:
                if midi_note in raw_assigned_notes_to_midi_assigned_notes(raw_assigned_notes):
                    return i
        return 0

    def set_assigned_notes_for_sound(self, sound_idx, assinged_notes, root_note=None):
        # assigned_notes here is a list with the midi note numbers
        assinged_notes = sorted(list(set(assinged_notes)))  # make sure they're all sorted and there are no duplicates
        if root_note is None:
            if assinged_notes:
                root_note = assinged_notes[len(assigned_notes)//2]  # Default to the middle note of the assigned notes
            else:
                root_note = 69
        if sound_idx > -1:
            assinged_notes_aux = ['0'] * 128
            for note in assinged_notes:
                assinged_notes_aux[note] = '1'
            midi_notes = hex(int("".join(reversed(''.join(assinged_notes_aux))), 2))  # Convert to a format expected by send_add_or_replace_sound_to_plugin
            sm.send_osc_to_plugin('/set_assigned_notes', [sound_idx, midi_notes, root_note])

    def remove_sound(self, sound_idx):
        sm.send_osc_to_plugin('/remove_sound', [sound_idx])
        sm.show_global_message("Sound removed!")

    def on_activating_state(self):
        # Called when state gets activated by the state manager (when it becomes visible)
        pass

    def on_deactivating_state(self):
        # Called right before the state gets deactivated by the state manager
        pass

    def on_source_state_update(self):
        # Called everytime the state manager gets an updated version of the APP state dict
        pass

    def on_encoder_rotated(self, direction, shift=False):
        #print('- Encoder rotated ({0}) in state {1} (shift={2})'.format(direction, self.name, shift))
        pass

    def on_encoder_pressed(self, shift=False):
        #print('- Encoder pressed in state {0} (shift={1})'.format(self.name, shift))
        pass

    def on_encoder_double_pressed(self, shift=False):
        #print('- Encoder double pressed in state {0} (shift={1})'.format(self.name, shift))
        pass

    def on_encoder_long_pressed(self, shift=False):
        #print('- Encoder long pressed in state {0} (shift={1})'.format(self.name, shift))
        pass

    def on_encoder_down(self, shift=False):
        #print('- Encoder down in state {0} (shift={1})'.format(self.name, shift))
        pass

    def on_encoder_up(self, shift=False):
        #print('- Encoder up in state {0} (shift={1})'.format(self.name, shift))
        pass
    
    def on_button_pressed(self, button_idx, shift=False):
        #print('- Button pressed ({0}) in state {1} (shift={2})'.format(button_idx, self.name, shift))
        pass

    def on_button_double_pressed(self, button_idx, shift=False):
        #print('- Button double pressed ({0}) in state {1} (shift={2})'.format(button_idx, self.name, shift))
        pass

    def on_button_long_pressed(self, button_idx, shift=False):
        #print('- Button long pressed ({0}) in state {1} (shift={2})'.format(button_idx, self.name, shift))
        pass
    
    def on_button_down(self, button_idx, shift=False):
        #print('- Button down ({0}) in state {1} (shift={2})'.format(button_idx, self.name, shift))
        pass

    def on_button_up(self, button_idx, shift=False):
        #print('- Button up ({0}) in state {1} (shift={2})'.format(button_idx, self.name, shift))
        pass

    def on_fader_moved(self, fader_idx, value, shift=False):
        #print('- Fader moved ({0}, {1}) in state {2} (shift={3})'.format(fader_idx, value, self.name, shift))
        pass


class PaginatedState(State):

    current_page = 0
    pages = [None]

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.current_page = kwargs.get('current_page', 0)
        if self.current_page >= len(self.pages):
            self.current_page = 0
    
    @property
    def num_pages(self):
        return len(self.pages)

    @property
    def current_page_data(self):
        return self.pages[self.current_page]

    def get_properties(self):
        properties = super().get_properties().copy()
        properties.update({
           'current_page': self.current_page
        })
        return properties

    def next_page(self):
        self.current_page += 1
        if self.current_page >= self.num_pages:
            self.current_page = 0

    def previous_page(self):
        self.current_page -= 1
        if self.current_page < 0:
            self.current_page = self.num_pages - 1

    def draw_scroll_bar(self, frame):
        return add_scroll_bar_to_frame(frame, self.current_page, self.num_pages)

    def on_encoder_rotated(self, direction, shift=False):
        if direction > 0:
            self.next_page()
        else:
            self.previous_page()


class GoBackOnEncoderLongPressedStateMixin(object):

    def on_encoder_long_pressed(self, shift=False):
        if not self.should_show_help():
            sm.go_back()
        else:
            self.exit_help()


class ShowHelpPagesMixin(object):

    def on_button_pressed(self, button_idx, shift=False):
        # Activate/deactivate help mode and cycle through help pages
        if button_idx == self.trigger_help_page_button_id:
            if self.help_pages:
                if self.current_help_page == None:
                    self.current_help_page = 0
                else:
                    self.current_help_page += 1
                    if self.current_help_page >= len(self.help_pages):
                        self.current_help_page = None
        else:
            super().on_button_pressed(button_idx=button_idx, shift=shift)


class ChangePresetOnEncoderShiftRotatedStateMixin(object):

    def on_encoder_rotated(self, direction, shift=False):
        if shift:
            # Change current loaded preset
            current_preset_index = sm.source_state.get(StateNames.LOADED_PRESET_INDEX, -1)
            if current_preset_index > -1:
                current_preset_index += direction
                if current_preset_index < 0:
                    current_preset_index = 0
                sm.send_osc_to_plugin("/load_preset", [current_preset_index])
        else:
            super().on_encoder_rotated(direction, shift=True)


class HomeState(ChangePresetOnEncoderShiftRotatedStateMixin, PaginatedState):

    pages = [EXTRA_PAGE_2_NAME] + sound_parameter_pages + [EXTRA_PAGE_1_NAME]

    def draw_display_frame(self):
        n_sounds = sm.source_state.get(StateNames.NUM_SOUNDS, 0)
        n_sounds_downloading = sm.source_state.get(StateNames.NUM_SOUNDS_DOWNLOADING, 0)
        n_sounds_loaded_in_sampler = sm.source_state.get(StateNames.NUM_SOUNDS_LOADED_IN_SAMPLER, 0)
        
        text = "{0} sounds".format(n_sounds)
        if n_sounds_loaded_in_sampler < n_sounds:
            text += " ({} loading)".format(n_sounds - n_sounds_loaded_in_sampler)
        lines = [{
            "underline": True, 
            "text":  text,
        }]

        if self.current_page_data == EXTRA_PAGE_1_NAME:
            # Show some sound information
            lines += [
                justify_text('Temp:', '{0}ºC'.format(sm.source_state[StateNames.SYSTEM_STATS].get("temp", ""))),
                justify_text('Memory:', '{0}%'.format(sm.source_state[StateNames.SYSTEM_STATS].get("mem", ""))),
                justify_text('CPU:', '{0}% | {1:.1f}%'.format(sm.source_state[StateNames.SYSTEM_STATS].get("cpu", ""), 
                                                              sm.source_state[StateNames.SYSTEM_STATS].get("xenomai_cpu", 0.0))), 
                justify_text('Network:', '{0}'.format(sm.source_state[StateNames.SYSTEM_STATS].get("network_ssid", "-")))
            ]
        elif self.current_page_data == EXTRA_PAGE_2_NAME:
            # Show some volatile state informaion (meters and voice activations)
            lines += ['L:', 'R:']
        else:
            # Show page parameter values
            for parameter_name in self.current_page_data:
                if parameter_name is not None:
                    _, get_func, parameter_label, value_label_template, _ = sound_parameters_info_dict[parameter_name]
                    
                    # Check if all loaded sounds have the same value for that parameter. If that is the case, show the value, otherwise don't show any value
                    all_sounds_values = []
                    last_value = None
                    for sound_idx in range(0, sm.source_state.get(StateNames.NUM_SOUNDS, 0)):
                        raw_value = sm.gsparam(sound_idx, parameter_name, default=None)
                        if raw_value is not None:
                            processed_val = get_func(raw_value)
                            all_sounds_values.append(processed_val)
                            last_value = processed_val

                    if len(set(all_sounds_values)) == 1:
                        # All sounds have the same value for that parameter, show the number
                        lines.append(justify_text(
                            parameter_label + ":", 
                            value_label_template.format(last_value)
                        ))
                    else:
                        # Some sounds differ, don't show the number
                        lines.append(justify_text(parameter_label + ":", "-"))
                else:
                    # Add blank line
                    lines.append("")
        
        frame = frame_from_lines([self.get_default_header_line()] + lines)
        if self.current_page_data == EXTRA_PAGE_2_NAME:
            frame = add_meter_to_frame(frame, y_offset_lines=3, value=sm.source_state.get(StateNames.METER_R, 0))
            frame = add_meter_to_frame(frame, y_offset_lines=2, value=sm.source_state.get(StateNames.METER_L, 0))
            voice_sound_idxs = sm.source_state.get(StateNames.VOICE_SOUND_IDXS, [])
            if voice_sound_idxs:
                frame = add_voice_grid_to_frame(frame, voice_activations=voice_sound_idxs)
            return frame  # Return before adding scrollbar if we're in first page
        return self.draw_scroll_bar(frame)

    def on_activating_state(self):
        sm.unset_all_leds()

    def on_button_pressed(self, button_idx, shift=False):
        # Select a sound
        sound_idx = self.get_sound_idx_from_buttons(button_idx, shift=shift)
        if sound_idx > -1:
            sm.move_to(SoundSelectedState(sound_idx))

    def on_encoder_pressed(self, shift=False):
        if sm.should_show_start_animation is True:
            # Stop start animation
            sm.should_show_start_animation = False
        else:
            # Open home contextual menu
            sm.move_to(HomeContextualMenuState())

    def on_encoder_double_pressed(self, shift=False):
        self.current_page = 0  # Move to first page

    def on_fader_moved(self, fader_idx, value, shift=False):
        if self.current_page_data in [EXTRA_PAGE_1_NAME, EXTRA_PAGE_2_NAME]:
            pass
        else:
            # Set sound parameters for all sounds
            parameter_name = self.current_page_data[fader_idx]
            if parameter_name is not None:
                send_func, _, _, _, osc_address = sound_parameters_info_dict[parameter_name]
                send_value = send_func(value)
                if shift and parameter_name == "pitch" or shift and parameter_name == "gain" or shift and parameter_name == "mod2PitchAmt":
                    send_value = send_value * 0.3333333  # Reduced range mode
                sm.send_osc_to_plugin(osc_address, [-1, parameter_name, send_value])


class SoundSelectedState(GoBackOnEncoderLongPressedStateMixin, PaginatedState):

    pages = [EXTRA_PAGE_1_NAME, EXTRA_PAGE_2_NAME]
    sound_idx = -1
    selected_sound_is_playing = False

    def __init__(self, sound_idx, *args, **kwargs):
        num_sounds = sm.source_state.get(StateNames.NUM_SOUNDS, 0)
        if sound_idx < 0:
            self.sound_idx = 0
        elif sound_idx >= num_sounds - 1:
            self.sound_idx = num_sounds - 1
        else:
            self.sound_idx = sound_idx
        if self.sound_is_loaded_in_sampler():
            self.pages += sound_parameter_pages
        super().__init__(*args, **kwargs)

    def sound_is_downloading(self):
        if sm.gsp(self.sound_idx, StateNames.SOUND_OGG_URL, default=''):
            return sm.gsp(self.sound_idx, StateNames.SOUND_DOWNLOAD_PROGRESS, default='0') != '100'
        else:
            # If sound does not have property SOUND_OGG_URL, then it is not a Freesound sound that
            # should be downloaded but a localfile, therefore it should be already in disk
            return False

    def sound_is_loaded_in_sampler(self):
        return sm.gsp(self.sound_idx, StateNames.SOUND_LOADED_IN_SAMPLER, default=False)

    def get_properties(self):
        properties = super().get_properties().copy()
        properties.update({
            'sound_idx': self.sound_idx
        })
        return properties

    def draw_display_frame(self):
        lines = [{
            "underline": True, 
            "move": True,
            "text": "S{0}:{1}".format(self.sound_idx + 1, sm.gsp(self.sound_idx, StateNames.SOUND_NAME))
        }]

        if self.current_page_data == EXTRA_PAGE_1_NAME:
            # Show some sound information
            sound_id = sm.gsp(self.sound_idx, StateNames.SOUND_ID)
            lines += [
                justify_text('ID:', '{0}'.format(sound_id) if sound_id != "-1" else "-"),
                justify_text('User:', '{0}'.format(sm.gsp(self.sound_idx, StateNames.SOUND_AUTHOR))),
                justify_text('License:', '{0}'.format(sm.gsp(self.sound_idx, StateNames.SOUND_LICENSE)))
            ]
            if self.sound_is_loaded_in_sampler():
                lines += [justify_text('Duration:', '{0:.2f}s'.format(sm.gsp(self.sound_idx, StateNames.SOUND_DURATION)))]
            else:
                sound_download_progress = int(sm.gsp(self.sound_idx, StateNames.SOUND_DOWNLOAD_PROGRESS, default=0))
                if self.sound_is_downloading():
                    if sound_download_progress < 95:
                        lines += [justify_text('Downloading...', '{0}%'.format(sound_download_progress))]
                    else:
                        lines += ['Loading in sampler...']
                else:
                    # If sound is not loaded in sampler and is not downloading it can be that:
                    # 1) sound comes from a local file (not from a Freesound download, even if it was previosuly downloaded) and is being loaded right now
                    # 2) sound comes from a local file (not from a Freesound download, even if it was previosuly downloaded) and it can't be found on disk (or there were errors loading)
                    if sm.gsp(self.sound_idx, StateNames.SOUND_LOCAL_FILE_PATH, default='') and not os.path.exists(sm.gsp(self.sound_idx, StateNames.SOUND_LOCAL_FILE_PATH)):
                        lines += ['Local file not found...']
                    else:
                        # If there are errors loading, this will always show 'Loading in sampler...'
                        # This could be improved in the future
                        lines += ['Loading in sampler...']

        elif self.current_page_data == EXTRA_PAGE_2_NAME:
            # Show other sound information
            using_preview_marker = '*' if sm.gsp(self.sound_idx, StateNames.SOUND_LOADED_PREVIEW_VERSION, default=True) and self.sound_is_loaded_in_sampler() else ''
            lines += [
                justify_text('Type:', '{}{}'.format(sm.gsp(self.sound_idx, StateNames.SOUND_TYPE, default='-').upper(), using_preview_marker)),
                justify_text('Size:', '{}{}'.format(sizeof_fmt(sm.gsp(self.sound_idx, StateNames.SOUND_FILESIZE, default=0)), using_preview_marker))
            ]
        else:
            # Show page parameter values
            for parameter_name in self.current_page_data:
                if parameter_name is not None:
                    _, get_func, parameter_label, value_label_template, _ = sound_parameters_info_dict[parameter_name]
                    raw_value = sm.gsparam(self.sound_idx, parameter_name, default=None)
                    if raw_value is not None:
                        parameter_value_label = value_label_template.format(get_func(raw_value))
                    else:
                        parameter_value_label = "-"

                    lines.append(justify_text(
                        parameter_label + ":", 
                        parameter_value_label
                    ))
                else:
                    # Add blank line
                    lines.append("")

        return self.draw_scroll_bar(frame_from_lines([self.get_default_header_line()] + lines))

    def play_selected_sound(self):
        if self.selected_sound_is_playing:
            self.stop_selected_sound()
        sm.send_osc_to_plugin("/play_sound", [self.sound_idx])
        self.selected_sound_is_playing = True

    def stop_selected_sound(self):
        if self.selected_sound_is_playing:
            sm.send_osc_to_plugin("/stop_sound", [self.sound_idx])
            self.selected_sound_is_playing = False

    def on_activating_state(self):
        sm.set_led((self.sound_idx % 8) + 1, unset_others=True)

    def on_deactivating_state(self):
        self.stop_selected_sound()

    def on_source_state_update(self):
        # Check that self.sound_idx is in range with the new state, otherwise change the state to a new state with valid self.sound_idx
        num_sounds = sm.source_state.get(StateNames.NUM_SOUNDS, 0)
        if num_sounds == 0:
            sm.go_back()
        else:
            if self.sound_idx >= num_sounds:
                sm.move_to(SoundSelectedState(num_sounds -1, current_page=self.current_page), replace_current=True)
            elif self.sound_idx < 0 and num_sounds > 0:
                sm.move_to(SoundSelectedState(0, current_page=self.current_page), replace_current=True)
            else:
                if self.sound_is_loaded_in_sampler():
                    # If sound is already loaded, show all pages
                    self.pages = [EXTRA_PAGE_1_NAME, EXTRA_PAGE_2_NAME] + sound_parameter_pages
                else:
                    # Oterwise only show the first one
                    self.pages = [EXTRA_PAGE_1_NAME, EXTRA_PAGE_2_NAME]
                    if self.current_page > len(self.pages) - 1:
                        self.current_page = 0
    
    def on_button_pressed(self, button_idx, shift=False):
        # Stop current sound
        self.stop_selected_sound()
        
        # Select another sound
        sound_idx = self.get_sound_idx_from_buttons(button_idx, shift=shift)
        if sound_idx > -1:
            sm.move_to(SoundSelectedState(sound_idx, current_page=self.current_page), replace_current=True)

    def on_encoder_rotated(self, direction, shift=False):
        if shift:
            # Stop current sound
            self.stop_selected_sound()
            
            # Select another sound
            num_sounds = sm.source_state.get(StateNames.NUM_SOUNDS, 0)
            if num_sounds:
                sound_idx = self.sound_idx + direction
                if sound_idx < 0:
                    sound_idx = num_sounds - 1
                if sound_idx >= num_sounds:
                    sound_idx = 0
                sm.move_to(SoundSelectedState(sound_idx, current_page=self.current_page), replace_current=True)
        else:
            super().on_encoder_rotated(direction, shift=False)

    def on_button_double_pressed(self, button_idx, shift=False):
        # If button corresponds to current sound, play it
        sound_idx = self.get_sound_idx_from_buttons(button_idx, shift=shift)
        if sound_idx > -1:
            if sound_idx == self.sound_idx:
                self.stop_selected_sound()
                self.play_selected_sound()
    
    def on_encoder_pressed(self, shift=False):
        if not shift:
            # Open sound contextual menu
            sm.move_to(SoundSelectedContextualMenuState(sound_idx=self.sound_idx, sound_finished_loading=self.sound_is_loaded_in_sampler()))
        else:
            # Trigger the selected sound
            self.play_selected_sound()

    def on_fader_moved(self, fader_idx, value, shift=False):
        if self.current_page_data in [EXTRA_PAGE_1_NAME, EXTRA_PAGE_2_NAME]:
            pass
        else:
            # Set sound parameters for selected sound
            parameter_name = self.current_page_data[fader_idx]
            if parameter_name is not None:
                send_func, _, _, _, osc_address = sound_parameters_info_dict[parameter_name]
                send_value = send_func(value)
                if shift and parameter_name == "pitch" or shift and parameter_name == "gain":
                    send_value = send_value * 0.3333333  # Reduced range mode
                sm.send_osc_to_plugin(osc_address, [self.sound_idx, parameter_name, send_value])


class MenuState(State):

    page_size = 4
    selected_item = 0
    items = ["Item"]

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.selected_item = kwargs.get('selected_item', 0)
    
    @property
    def num_items(self):
        return len(self.items)

    @property
    def selected_item_name(self):
        return self.items[self.selected_item]

    def get_properties(self):
        properties = super().get_properties().copy()
        properties.update({
           'selected_item': self.selected_item
        })
        return properties

    def next_item(self):
        self.selected_item += 1
        if self.selected_item >= self.num_items:
            self.selected_item = self.num_items -1

    def previous_item(self):
        self.selected_item -= 1
        if self.selected_item < 0:
            self.selected_item = 0

    def get_menu_item_lines(self):
        lines = []
        current_page = self.selected_item // self.page_size
        for item in self.items[current_page * self.page_size:(current_page + 1) * self.page_size]:
            lines.append({
                "invert": True if item == self.selected_item_name else False, 
                "text": item
            })
        return lines

    def on_encoder_rotated(self, direction, shift=False):
        if direction > 0:
            self.next_item()
            if shift:
                for i in range(0, 9):
                    self.next_item()
        else:
            self.previous_item()
            if shift:
                for i in range(0, 9):
                    self.previous_item()

    def on_encoder_pressed(self, shift=False):
        self.perform_action(self.selected_item_name)

    def perform_action(self, action_name):
        pass


class MenuCallbackState(ShowHelpPagesMixin, GoBackOnEncoderLongPressedStateMixin, MenuState):

    callback = None  # Triggered when encoder pressed
    shift_callback = None  # Triggered when shift+encoder pressed
    update_items_callback = None  # Callback to update available items when state is updated
    extra_data_for_callback = {}
    title1 = "NoTitle1"
    title2 = "NoTitle2"
    page_size = 4
    go_back_n_times = 0
    go_back_n_times_shift_callback = 0

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.selected_item = kwargs.get('selected_item', 0)
        self.items = kwargs.get('items', [])
        self.title1 = kwargs.get('title1', None)
        self.title2 = kwargs.get('title2', None)
        self.page_size = kwargs.get('page_size', 4)
        self.callback = kwargs.get('callback', None)
        self.shift_callback = kwargs.get('shift_callback', None)
        self.extra_data_for_callback = kwargs.get('extra_data_for_callback', None)
        self.go_back_n_times = kwargs.get('go_back_n_times', 0)
        self.go_back_n_times_shift_callback = kwargs.get('go_back_n_times_shift_callback', 0)
        self.help_pages = kwargs.get('help_pages', [])
        self.update_items_callback = kwargs.get('update_items_callback', None)

    def draw_display_frame(self):
        lines = [{
            "underline": True, 
            "text": self.title1
        }]
        if self.page_size == 3:
            # Page size of 3 gives space for ane extra title
            lines.append({
                "underline": True, 
                "text": self.title2
            })
        lines += self.get_menu_item_lines()
        return frame_from_lines([self.get_default_header_line()] + lines)

    def on_encoder_pressed(self, shift=False):
        if self.callback is not None:
            if self.shift_callback is None:
                if self.extra_data_for_callback is None:
                    self.callback(self.selected_item_name)
                else:
                    self.callback(self.selected_item_name, **self.extra_data_for_callback)
                sm.go_back(n_times=self.go_back_n_times)
            else:
                if shift:
                    self.shift_callback(self.selected_item_name)
                    sm.go_back(n_times=self.go_back_n_times_shift_callback)
                else:
                    if self.extra_data_for_callback is None:
                        self.callback(self.selected_item_name)
                    else:
                        self.callback(self.selected_item_name, **self.extra_data_for_callback)
                    sm.go_back(n_times=self.go_back_n_times)

    def on_source_state_update(self):
        if self.update_items_callback is not None:
            self.items = self.update_items_callback()
            if self.selected_item >= len(self.items):
                self.selected_item = len(self.items) - 1


class ReverbSettingsMenuState(GoBackOnEncoderLongPressedStateMixin, PaginatedState):

    pages = reverb_parameters_pages

    def draw_display_frame(self):
        lines = [{
            "underline": True, 
            "text": "Reverb settings..."
        }]

        # Show page parameter values
        for parameter_name in self.current_page_data:
            if parameter_name is not None:
                parameter_position, parameter_label = reverb_parameters_info_dict[parameter_name]
                reverb_params = sm.source_state.get(StateNames.REVERB_SETTINGS, [0.0, 0.0, 0.0, 0.0, 0.0, 0.0])
                lines.append(justify_text(
                    parameter_label + ":", 
                    '{0:.2f}'.format(reverb_params[parameter_position])
                ))
            else:
                # Add blank line
                lines.append("")

        return self.draw_scroll_bar(frame_from_lines([self.get_default_header_line()] + lines))
 
    def on_encoder_pressed(self, shift=False):
        sm.go_back()

    def on_fader_moved(self, fader_idx, value, shift=False):
        parameter_name = self.current_page_data[fader_idx]
        if parameter_name is not None:
            param_position, _ = reverb_parameters_info_dict[parameter_name]
            reverb_params = sm.source_state.get(StateNames.REVERB_SETTINGS, [0.0, 0.0, 0.0, 0.0, 0.0, 0.0])
            reverb_params[param_position] = value
            sm.send_osc_to_plugin("/set_reverb_parameters", reverb_params)


class EnterQuerySettingsState(ShowHelpPagesMixin, GoBackOnEncoderLongPressedStateMixin, PaginatedState):

    callback = None
    extra_data_for_callback = None
    allow_change_num_sounds = True
    allow_change_layout = True
    title = ""
    last_recent_loaded = 0
    help_pages = [[
        "B1 load recent"
    ]]

    query_settings_values = {}
    pages = query_settings_pages

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.title = kwargs.get('title', "Query filters") 

        # Set default parameters
        for parameter_name in query_settings_info_dict.keys():
            self.query_settings_values[parameter_name] = query_settings_info_dict[parameter_name][3]
        # Overwrite num sounds if passed in query
        self.query_settings_values['num_sounds'] = kwargs.get('num_sounds', 16)      
        self.callback = kwargs.get('callback', None)
        self.extra_data_for_callback = kwargs.get('extra_data_for_callback', None)
        self.allow_change_num_sounds = kwargs.get('allow_change_num_sounds', True)
        self.allow_change_layout = kwargs.get('allow_change_layout', True)

    def parameter_is_allowed(self, parameter_name):
        if parameter_name == 'num_sounds' and not self.allow_change_num_sounds:
            return False
        if parameter_name == 'layout' and not self.allow_change_layout:
            return False
        return True
    
    def draw_display_frame(self):
        lines = [{
            "underline": True, 
            "text": self.title
            }
        ]
        for parameter_name in self.current_page_data:
            if parameter_name is not None:
                _, label, value_template, default = query_settings_info_dict[parameter_name]
                value = self.query_settings_values.get(parameter_name, default)
                lines += [justify_text(label + ':', value_template.format(value) if self.parameter_is_allowed(parameter_name) else '-')]
            else:
                lines += [""]
       
        return self.draw_scroll_bar(frame_from_lines([self.get_default_header_line()] + lines))

    def on_encoder_pressed(self, shift=False):
        # Move to query chooser state
        if self.callback is not None:
            query_settings = self.query_settings_values.copy()
            query_settings.update({'layout': note_layout_types.index(query_settings['layout'])})  # Change the format of layout parameter
            add_recent_query_filter(query_settings)  # Store the filter settings
            callback_data = {'query_settings': query_settings}
            if self.extra_data_for_callback is not None:
                callback_data.update(self.extra_data_for_callback)
            self.callback(**callback_data)

    def on_fader_moved(self, fader_idx, value, shift=False):
        parameter_name = self.current_page_data[fader_idx]
        if parameter_name is not None and self.parameter_is_allowed(parameter_name):
            send_func, _, _, _ = query_settings_info_dict[parameter_name]
            self.query_settings_values[parameter_name] = send_func(value)

    def on_button_pressed(self, button_idx, shift=False):
        if button_idx == 1:
            # Load one of the recent stored query filters (if any)
            recent_query_filters = get_recent_query_filters()
            if recent_query_filters:
                recent_filter = recent_query_filters[self.last_recent_loaded % len(recent_query_filters)].copy()
                recent_filter.update({'layout': note_layout_types[recent_filter['layout']]})
                sm.show_global_message('Loaded recent\n qeury filters ({})'.format(self.last_recent_loaded % len(recent_query_filters) + 1), duration=0.5)
                self.query_settings_values = recent_filter
                self.last_recent_loaded += 1
        else:
            super().on_button_pressed(button_idx=button_idx, shift=shift)  # Needed for reach help page mixin


class NewPresetOptionsMenuState(GoBackOnEncoderLongPressedStateMixin, MenuState):

    OPTION_BY_QUERY = "From new query"
    OPTION_BY_PREDEFINED_QUERY = "From predefined query"
    OPTION_BY_SIMILARITY = "By sound similarity"
    OPTION_RANDOM = "Random sounds"
    
    items = [OPTION_BY_QUERY, OPTION_BY_PREDEFINED_QUERY, OPTION_BY_SIMILARITY, OPTION_RANDOM]
    page_size = 4

    def draw_display_frame(self):
        lines = [{
            "underline": True, 
            "text": "Get new sounds..."
        }]
        lines += self.get_menu_item_lines()
        return frame_from_lines([self.get_default_header_line()] + lines)

    def perform_action(self, action_name):
        if action_name == self.OPTION_BY_QUERY:
            sm.move_to(EnterQuerySettingsState(
                callback=lambda query_settings: sm.move_to(
                    EnterTextViaHWOrWebInterfaceState(
                        title="Enter query",
                        web_form_id="enterQuery", 
                        callback=self.new_preset_by_query, 
                        extra_data_for_callback=query_settings,
                        go_back_n_times=4
                    )
                )
            ))
        elif action_name == self.OPTION_BY_PREDEFINED_QUERY:
            sm.move_to(EnterQuerySettingsState(
                callback=lambda query_settings: sm.move_to(
                    MenuCallbackState(
                        items=predefined_queries, 
                        selected_item=random.choice(range(0, len(predefined_queries))), 
                        title1="Select query",
                        callback=self.new_preset_from_predefined_query, 
                        extra_data_for_callback=query_settings,
                        go_back_n_times=4
                    )
                )
            ))
        elif action_name == self.OPTION_RANDOM:
            sm.move_to(EnterQuerySettingsState(
                callback=lambda query_settings: (self.new_preset_by_random_sounds(**query_settings), sm.go_back(n_times=3))  
            ))
        elif action_name == self.OPTION_BY_SIMILARITY:
            current_note_mapping_type = sm.source_state.get(StateNames.NOTE_LAYOUT_TYPE, 1)
            self.new_preset_by_similar_sounds(note_mapping_type=current_note_mapping_type)
            sm.go_back(n_times=3)
        else:
            sm.show_global_message('Not implemented...')


class NewSoundOptionsMenuState(GoBackOnEncoderLongPressedStateMixin, MenuState):

    OPTION_BY_QUERY = "From new query"
    OPTION_RANDOM = "Random sound"
    OPTION_FROM_BOOKMARK = "From bookmarks"
    OPTION_LOCAL = "Load from disk"
    
    items = [OPTION_BY_QUERY, OPTION_RANDOM, OPTION_LOCAL]
    page_size = 4

    def draw_display_frame(self):
        if is_logged_in() and self.OPTION_FROM_BOOKMARK not in self.items:
            self.items.append(self.OPTION_FROM_BOOKMARK)
        if not is_logged_in() and self.OPTION_FROM_BOOKMARK in self.items:
            self.items = [item for item in self.items if item != self.OPTION_FROM_BOOKMARK]

        lines = [{
            "underline": True, 
            "text": "Add new sound..."
        }]
        lines += self.get_menu_item_lines()
        return frame_from_lines([self.get_default_header_line()] + lines)

    def perform_action(self, action_name):
        if action_name == self.OPTION_BY_QUERY:
            sm.move_to(EnterQuerySettingsState(
                callback=lambda query_settings: 
                    sm.move_to(
                        EnterTextViaHWOrWebInterfaceState(
                            title="Enter query",
                            web_form_id="enterQuery", 
                            callback=self.add_or_replace_sound_by_query, 
                            extra_data_for_callback=merge_dicts(query_settings, {'move_once_loaded': True}),
                            go_back_n_times=4
                    )),
                    num_sounds=1,
                    allow_change_num_sounds=False,
                    allow_change_layout=False), 
                )  # Use big number so we move to last sound loaded
        elif action_name == self.OPTION_RANDOM:
            sm.move_to(EnterQuerySettingsState(
                callback=lambda query_settings: (self.add_or_replace_sound_random(**merge_dicts(query_settings, {'move_once_loaded': True})), sm.go_back(n_times=3)),
                num_sounds=1,
                allow_change_num_sounds=False,
                allow_change_layout=False
            ))
        elif action_name == self.OPTION_LOCAL:
            base_path = get_local_audio_files_path()
            if base_path is not None:
                available_files_ogg, _, _ = get_filenames_in_dir(base_path, '*.ogg')
                available_files_wav, _, _ = get_filenames_in_dir(base_path, '*.wav')
                available_files = sorted(available_files_ogg + available_files_wav)
                sm.move_to(FileChooserState(
                    base_path=base_path,
                    items=available_files,
                    title1="Select a file...",
                    go_back_n_times=3,
                    callback=lambda file_path: (self.send_add_or_replace_sound_to_plugin(
                        -1,  # -1 to add new sound 
                        {
                            'id': -1, 
                            'name': file_path.split('/')[-1], 
                            'username': '-',
                            'license': '-',
                            'previews': {'preview-hq-ogg': ''},
                        }, 
                        local_file_path=file_path,
                        move_once_loaded=True
                    ), sm.show_global_message('Loading file\n{}...'.format(file_path.split('/')[-1]))),
                ))
            else:
                sm.show_global_message('Local files\nnot ready...')
        elif action_name == self.OPTION_FROM_BOOKMARK:
            sm.show_global_message('Getting\nbookmarks...')
            user_bookmarks = get_user_bookmarks()
            sm.show_global_message('')
            sounds_data = {item['name']:item for item in user_bookmarks}
            sm.move_to(SoundChooserState(
                    items=[item['name'] for item in user_bookmarks],
                    sounds_data=sounds_data,
                    title1="Select a sound...",
                    go_back_n_times=3,
                    callback=lambda sound_name: (self.send_add_or_replace_sound_to_plugin(
                        -1,  # -1 to add new sound 
                        sounds_data[sound_name],
                        move_once_loaded=True
                    ), sm.show_global_message('Loading file\n{}...'.format(sound_name))),
                ))

class HomeContextualMenuState(GoBackOnEncoderLongPressedStateMixin, MenuState):

    OPTION_SAVE = "Save preset"
    OPTION_SAVE_AS = "Save preset as..."
    OPTION_RELOAD = "Reload preset"
    OPTION_NEW_SOUNDS = "Get new sounds..."
    OPTION_ADD_NEW_SOUND = "Add new sound..."
    OPTION_REVERB = "Reverb settings..."
    OPTION_RELAYOUT = "Apply note layout..."
    OPTION_NUM_VOICES = "Set num voices..."
    OPTION_LOAD_PRESET = "Load preset..."
    OPTION_GO_TO_SOUND = "Go to sound..."
    OPTION_SOUND_USAGE_LOG = "Sound usage log..."
    OPTION_LOGIN_TO_FREESOUND = "Login to Freesound"
    OPTION_LOGOUT_FROM_FREESOUND = "Logout from Freesound"
    OPTION_DOWNLOAD_ORIGINAL = "Use original files..."

    items = [OPTION_SAVE, OPTION_SAVE_AS, OPTION_RELOAD, OPTION_LOAD_PRESET, \
             OPTION_NEW_SOUNDS, OPTION_ADD_NEW_SOUND, OPTION_RELAYOUT, OPTION_REVERB, \
             OPTION_NUM_VOICES, OPTION_GO_TO_SOUND, OPTION_SOUND_USAGE_LOG, OPTION_DOWNLOAD_ORIGINAL, OPTION_LOGIN_TO_FREESOUND]
    page_size = 5

    def draw_display_frame(self):
        if is_logged_in() and self.OPTION_LOGOUT_FROM_FREESOUND not in self.items:
            self.items.append(self.OPTION_LOGOUT_FROM_FREESOUND)
        if not is_logged_in() and self.OPTION_LOGOUT_FROM_FREESOUND in self.items:
            self.items = [item for item in self.items if item != self.OPTION_LOGOUT_FROM_FREESOUND]

        lines = self.get_menu_item_lines()
        
        # Add FS username 
        for line in lines:
            if line['text'] == self.OPTION_LOGOUT_FROM_FREESOUND:
                line['text'] = 'Logout ({})'.format(get_crurrently_logged_in_user())

        return frame_from_lines([self.get_default_header_line()] + lines)

    def get_exising_presets_list(self):
        # Scan existing .xml files to know preset names
        # TODO: the presetting system should be imporved so we don't need to scan the presets folder every time
        preset_names = {}
        presets_folder = sm.source_state.get(StateNames.PRESETS_DATA_LOCATION, None)
        if presets_folder is not None:
            for filename in os.listdir(presets_folder):
                if filename.endswith('.xml'):
                    try:
                        preset_id = int(filename.split('.xml')[0])
                    except ValueError:
                        # Not a valid preset file
                        continue
                    file_contents = open(os.path.join(presets_folder, filename), 'r').read()
                    preset_name = file_contents.split('SourcePresetState presetName="')[1].split('"')[0]
                    preset_names[preset_id] = preset_name
        return preset_names

    def move_to_choose_preset_name_state(self, preset_idx_name):
        preset_idx = int(preset_idx_name.split(':')[0])
        current_preset_name = current_preset_index = sm.source_state.get(StateNames.LOADED_PRESET_NAME, "") # preset_idx_name.split(':')[1]
        sm.move_to(EnterTextViaHWOrWebInterfaceState(
                title="Preset name", 
                web_form_id="enterName", 
                callback=self.save_current_preset_to, 
                current_text=current_preset_name,
                store_recent=False,
                extra_data_for_callback={
                    'preset_idx': preset_idx,
                },
                go_back_n_times=4)) 

    def perform_action(self, action_name):
        if action_name == self.OPTION_SAVE:
            self.save_current_preset()
            sm.go_back()
        elif action_name == self.OPTION_SAVE_AS:
            preset_names = self.get_exising_presets_list()
            current_preset_index = sm.source_state.get(StateNames.LOADED_PRESET_INDEX, 0)
            if not preset_names:
                sm.show_global_message('Some error\noccurred...')
                sm.go_back()
                sm.move_to(EnterNumberState(initial=current_preset_index, minimum=0, maximum=127, title1="Load preset...", callback=self.load_preset, go_back_n_times=2))
            else:
                sm.move_to(MenuCallbackState(
                    items=['{0}:{1}'.format(i, preset_names.get(i, 'empty')) for i in range(0, 128)], 
                    selected_item=current_preset_index, 
                    title1="Where to save...", 
                    callback=self.move_to_choose_preset_name_state, 
                    go_back_n_times=0))

        elif action_name == self.OPTION_RELOAD:
            self.reload_current_preset()
            sm.go_back()
        elif action_name == self.OPTION_RELAYOUT:
            # see NOTE_MAPPING_TYPE_CONTIGUOUS in defines.h, contiguous and interleaved indexes must match with those in that file (0 and 1)
            sm.move_to(MenuCallbackState(items=note_layout_types, selected_item=0, title1="Apply note layout...", callback=self.reapply_note_layout, go_back_n_times=2))
        elif action_name == self.OPTION_NUM_VOICES:
            current_num_voices = sm.source_state.get(StateNames.NUM_VOICES, 1)
            sm.move_to(EnterNumberState(initial=current_num_voices, minimum=1, maximum=32, title1="Number of voices", callback=self.set_num_voices, go_back_n_times=2))
        elif action_name == self.OPTION_LOAD_PRESET:
            preset_names = self.get_exising_presets_list()
            current_preset_index = sm.source_state.get(StateNames.LOADED_PRESET_INDEX, 0)
            if not preset_names:
                sm.move_to(EnterNumberState(initial=current_preset_index, minimum=0, maximum=127, title1="Load preset...", callback=self.load_preset, go_back_n_times=2))
            else:
                sm.move_to(MenuCallbackState(items=['{0}:{1}'.format(i, preset_names.get(i, 'empty')) for i in range(0, 128)], selected_item=current_preset_index, title1="Load preset...", callback=self.load_preset, go_back_n_times=2))
        elif action_name == self.OPTION_REVERB:
            sm.move_to(ReverbSettingsMenuState())
        elif action_name == self.OPTION_ADD_NEW_SOUND:
            sm.move_to(NewSoundOptionsMenuState())
        elif action_name == self.OPTION_NEW_SOUNDS:
            sm.move_to(NewPresetOptionsMenuState())
        elif action_name == self.OPTION_GO_TO_SOUND:
            sm.move_to(EnterNoteState(
                title1="Go to sound...",
                callback=lambda note: (sm.go_back(n_times=2), sm.move_to(SoundSelectedState(self.get_sound_idx_from_note(note)))),
            ))
        elif action_name == self.OPTION_SOUND_USAGE_LOG:
            sm.show_global_message('Sound usage log\n opened in browser')
            sm.open_url_in_browser = '/usage_log'
            sm.go_back()
        elif action_name == self.OPTION_LOGIN_TO_FREESOUND:
            sm.show_global_message('Check login\nin browser')
            sm.open_url_in_browser = 'https://freesound.org/apiv2/oauth2/logout_and_authorize/?client_id={}&response_type=code'.format(FREESOUND_CLIENT_ID)
            sm.go_back()
        elif action_name == self.OPTION_LOGOUT_FROM_FREESOUND:
            logout_from_freesound()
            sm.show_global_message('Logged out\nfrom Freesound')
            sm.go_back()
        elif action_name == self.OPTION_DOWNLOAD_ORIGINAL:
            selected_item = ['never', 'onlyShort', 'always'].index(sm.source_state.get(StateNames.USE_ORIGINAL_FILES_PREFERENCE, 'never'))
            sm.move_to(MenuCallbackState(items=['Never', 'Only small', 'Always'], selected_item=selected_item, title1="Use original files...", callback=lambda x: self.set_download_original_files(x), go_back_n_times=2))
        else:
            sm.show_global_message('Not implemented...')


class ReplaceByOptionsMenuState(GoBackOnEncoderLongPressedStateMixin, MenuState):

    OPTION_BY_QUERY = "New query"
    OPTION_BY_SIMILARITY = "Find similar"
    OPTION_BY_RANDOM = "Random sound"
    OPTION_FROM_DISK = "Load from disk"
    OPTION_FROM_BOOKMARK = "From bookmark"

    sound_idx = -1
    items = [OPTION_BY_QUERY, OPTION_BY_SIMILARITY, OPTION_BY_RANDOM, OPTION_FROM_DISK]
    page_size = 3

    def __init__(self, sound_idx, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.sound_idx = sound_idx

    def get_properties(self):
        properties = super().get_properties().copy()
        properties.update({
            'sound_idx': self.sound_idx
        })
        return properties

    def draw_display_frame(self):
        if is_logged_in() and self.OPTION_FROM_BOOKMARK not in self.items:
            self.items.append(self.OPTION_FROM_BOOKMARK)
        if not is_logged_in() and self.OPTION_FROM_BOOKMARK in self.items:
            self.items = [item for item in self.items if item != self.OPTION_FROM_BOOKMARK]
            
        lines = [{
            "underline": True, 
            "move": True,
            "text": "S{0}:{1}".format(self.sound_idx + 1, sm.gsp(self.sound_idx, StateNames.SOUND_NAME))
        }, {
            "underline": True, 
            "text": "Replace by..."
        }]
        lines += self.get_menu_item_lines()
        return frame_from_lines([self.get_default_header_line()] + lines)

    def perform_action(self, action_name):
        if action_name == self.OPTION_BY_QUERY:
            sm.move_to(EnterQuerySettingsState(
                callback=lambda query_settings: sm.move_to(
                    EnterTextViaHWOrWebInterfaceState(
                        title="Enter query",
                        web_form_id="enterQuery", 
                        callback=self.add_or_replace_sound_by_query, 
                        extra_data_for_callback=merge_dicts(query_settings, {'sound_idx': self.sound_idx}),
                        go_back_n_times=4
                    )),
                num_sounds=1,
                allow_change_num_sounds=False,
                allow_change_layout=False
            ))
        elif action_name == self.OPTION_BY_SIMILARITY:
            selected_sound_id = sm.gsp(self.sound_idx, StateNames.SOUND_ID)
            if selected_sound_id != '-':
                self.replace_sound_by_similarity(self.sound_idx, selected_sound_id)
                sm.go_back(n_times=2)  # Go back 2 times because option is 2-levels deep in menu hierarchy
        elif action_name == self.OPTION_BY_RANDOM:
            sm.move_to(EnterQuerySettingsState(
                callback=lambda query_settings: (self.add_or_replace_sound_random(**merge_dicts(query_settings, {'sound_idx': self.sound_idx})), sm.go_back(n_times=3)),
                num_sounds=1,
                allow_change_num_sounds=False,
                allow_change_layout=False
            ))
        elif action_name == self.OPTION_FROM_DISK:
            base_path = get_local_audio_files_path()
            if base_path is not None:
                available_files_ogg, _, _ = get_filenames_in_dir(base_path, '*.ogg')
                available_files_wav, _, _ = get_filenames_in_dir(base_path, '*.wav')
                available_files = sorted(available_files_ogg + available_files_wav)
                sm.move_to(FileChooserState(
                    base_path=base_path,
                    items=available_files,
                    title1="Select a file...",
                    go_back_n_times=3,
                    callback=lambda file_path: (self.send_add_or_replace_sound_to_plugin(
                        self.sound_idx, 
                        {
                            'id': -1, 
                            'name': file_path.split('/')[-1], 
                            'username': '-',
                            'license': '-',
                            'previews': {'preview-hq-ogg': ''},
                        }, 
                        local_file_path=file_path
                    ), sm.show_global_message('Loading file\n{}...'.format(file_path.split('/')[-1]))),
                ))
            else:
                sm.show_global_message('Local files\nnot ready...')
        elif action_name == self.OPTION_FROM_BOOKMARK:
            sm.show_global_message('Getting\nbookmarks...')
            user_bookmarks = get_user_bookmarks()
            sm.show_global_message('')
            sounds_data = {item['name']:item for item in user_bookmarks}
            sm.move_to(SoundChooserState(
                    items=[item['name'] for item in user_bookmarks],
                    sounds_data=sounds_data,
                    title1="Select a sound...",
                    go_back_n_times=3,
                    callback=lambda sound_name: (self.send_add_or_replace_sound_to_plugin(
                        self.sound_idx,
                        sounds_data[sound_name]
                    ), sm.show_global_message('Loading file\n{}...'.format(sound_name))),
                ))
        
        else:
            sm.show_global_message('Not implemented...')


class SoundSelectedContextualMenuState(GoBackOnEncoderLongPressedStateMixin, MenuState):

    OPTION_REPLACE = "Replace by..."
    OPTION_ASSIGNED_NOTES = "Assigned notes..."
    OPTION_PRECISION_EDITOR = "Slices editor..."
    OPTION_MIDI_CC = "MIDI CC mappings..."
    OPTION_OPEN_IN_FREESOUND = "Open in Freesound"
    OPTION_DELETE = "Delete"
    OPTION_GO_TO_SOUND = "Go to sound..."
    OPTION_BOOKMARK = "Bookmark in Freesound"

    MIDI_CC_ADD_NEW_TEXT = "Add new..."

    sound_idx = -1
    items = [OPTION_REPLACE, OPTION_MIDI_CC, OPTION_ASSIGNED_NOTES, OPTION_PRECISION_EDITOR, OPTION_OPEN_IN_FREESOUND, OPTION_DELETE, OPTION_GO_TO_SOUND]
    page_size = 4

    def __init__(self, sound_idx, sound_finished_loading, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.sound_idx = sound_idx

        if not sound_finished_loading:
            # If sound is not yet loaded in sampler, don't show precision editor option
            self.items = [item for item in self.items if item not in [
                self.OPTION_PRECISION_EDITOR,
                self.OPTION_ASSIGNED_NOTES
            ]]

    def get_properties(self):
        properties = super().get_properties().copy()
        properties.update({
            'sound_idx': self.sound_idx
        })
        return properties

    def on_source_state_update(self):
        # Check that self.sound_idx is in range with the new state, otherwise change the state to a new state with valid self.sound_idx
        num_sounds = sm.source_state.get(StateNames.NUM_SOUNDS, 0)
        if self.sound_idx >= num_sounds:
            sm.move_to(SoundSelectedContextualMenuState(num_sounds -1), replace_current=True)
        elif self.sound_idx < 0 and num_sounds > 0:
            sm.move_to(SoundSelectedContextualMenuState(0), replace_current=True)

    def draw_display_frame(self):
        if is_logged_in() and self.OPTION_BOOKMARK not in self.items:
            self.items.append(self.OPTION_BOOKMARK)
        if not is_logged_in() and self.OPTION_BOOKMARK in self.items:
            self.items = [item for item in self.items if item != self.OPTION_BOOKMARK]

        lines = [{
            "underline": True, 
            "move": True,
            "text": "S{0}:{1}".format(self.sound_idx + 1, sm.gsp(self.sound_idx, StateNames.SOUND_NAME))
        }]
        lines += self.get_menu_item_lines()
        return frame_from_lines([self.get_default_header_line()] + lines)

    def perform_action(self, action_name):
        if action_name == self.OPTION_REPLACE:
            sm.move_to(ReplaceByOptionsMenuState(sound_idx=self.sound_idx))
        elif action_name == self.OPTION_DELETE:
            self.remove_sound(self.sound_idx)
            sm.go_back()
        elif action_name == self.OPTION_OPEN_IN_FREESOUND:
            sm.show_global_message('Sound opened\nin browser')
            selected_sound_id = sm.gsp(self.sound_idx, StateNames.SOUND_ID)
            if selected_sound_id != '-':
                sm.open_url_in_browser = 'https://freesound.org/s/{}'.format(selected_sound_id)
            sm.go_back()
        elif action_name == self.OPTION_PRECISION_EDITOR:
            sm.move_to(SoundSliceEditorState(sound_idx=self.sound_idx))
        elif action_name == self.OPTION_ASSIGNED_NOTES:
            sm.move_to(SoundAssignedNotesEditorState(sound_idx=self.sound_idx))
        elif action_name == self.OPTION_MIDI_CC:
            sm.move_to(
                MenuCallbackState(
                items=self.get_midi_cc_items_for_midi_cc_assignments_menu(), 
                selected_item=0, 
                title1="MIDI CC mappings", 
                callback=self.handle_select_midi_cc_assignment, 
                shift_callback=self.handle_delete_midi_cc_assignment,
                update_items_callback=self.get_midi_cc_items_for_midi_cc_assignments_menu,
                help_pages=[['Sh+Enc remove current']],
                go_back_n_times=0)  # Don't go back because we go into another menu and we handle this individually in the callbacks of the inside options
            )
        elif action_name == self.OPTION_GO_TO_SOUND:
            sm.move_to(EnterNoteState(
                title1="Go to sound...",
                callback=lambda note: (sm.go_back(n_times=3), sm.move_to(SoundSelectedState(self.get_sound_idx_from_note(note)))),
            ))
        elif action_name == self.OPTION_BOOKMARK:
            selected_sound_id = sm.gsp(self.sound_idx, StateNames.SOUND_ID)
            if selected_sound_id != '-':
                result = bookmark_sound(selected_sound_id)
                if result:
                    sm.show_global_message('Freesound\nbookmark added!')
                else:
                    sm.show_global_message('Error bookmarking\n sound')
            else:
                sm.show_global_message('Can\'t bookmark non\nFreesound sound')
            sm.go_back()
        else:
            sm.show_global_message('Not implemented...')

    def get_midi_cc_items_for_midi_cc_assignments_menu(self):
        return [label for label in sorted(sm.gsp(self.sound_idx, StateNames.SOUND_MIDI_CC_ASSIGNMENTS, default={}).keys())] + [self.MIDI_CC_ADD_NEW_TEXT]   

    def handle_select_midi_cc_assignment(self, midi_cc_assignment_label):
        if midi_cc_assignment_label == self.MIDI_CC_ADD_NEW_TEXT:
            # Show "new assignment" menu
            sm.move_to(EditMIDICCAssignmentState(sound_idx=self.sound_idx))
        else:
            midi_cc_assignment = sm.gsp(self.sound_idx, StateNames.SOUND_MIDI_CC_ASSIGNMENTS, default={}).get(midi_cc_assignment_label, None)
            if midi_cc_assignment is not None:
                # Show "edit assignment" menu
                sm.move_to(EditMIDICCAssignmentState(
                    cc_number = midi_cc_assignment[StateNames.SOUND_MIDI_CC_ASSIGNMENT_CC_NUMBER],
                    parameter_name = midi_cc_assignment[StateNames.SOUND_MIDI_CC_ASSIGNMENT_PARAM_NAME],
                    min_range = midi_cc_assignment[StateNames.SOUND_MIDI_CC_ASSIGNMENT_MIN_RANGE],
                    max_range = midi_cc_assignment[StateNames.SOUND_MIDI_CC_ASSIGNMENT_MAX_RANGE],
                    random_id = midi_cc_assignment[StateNames.SOUND_MIDI_CC_ASSIGNMENT_RANDOM_ID],
                    sound_idx = self.sound_idx
                ))

    def handle_delete_midi_cc_assignment(self, midi_cc_assignment_label):
        midi_cc_assignment = sm.gsp(self.sound_idx, StateNames.SOUND_MIDI_CC_ASSIGNMENTS, default={}).get(midi_cc_assignment_label, None)
        if midi_cc_assignment is not None:
            sm.send_osc_to_plugin('/remove_cc_mapping', [self.sound_idx, int(midi_cc_assignment[StateNames.SOUND_MIDI_CC_ASSIGNMENT_RANDOM_ID])])
            sm.show_global_message('Removing MIDI\nmapping...')

class EditMIDICCAssignmentState(GoBackOnEncoderLongPressedStateMixin, State):

    cc_number = -1
    parameter_name = ""
    min_range = 0.0
    max_range = 1.0
    random_id = -1
    sound_idx = -1
    available_parameter_names = midi_cc_available_parameters_list

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.cc_number = kwargs.get('cc_number', -1)  # Default to MIDI learn mode
        self.parameter_name = kwargs.get('parameter_name', self.available_parameter_names[-1])
        self.min_range = kwargs.get('min_range', 0.0)
        self.max_range = kwargs.get('max_range', 1.0)
        self.random_id = kwargs.get('random_id', -1)
        self.sound_idx = kwargs.get('sound_idx', -1)

    def draw_display_frame(self):
        if self.cc_number > -1:
            cc_number = str(self.cc_number)
        else:
            last_cc_received = int(sm.source_state.get(StateNames.LAST_CC_MIDI_RECEIVED, 0))
            if last_cc_received < 0:
                last_cc_received = 0
            cc_number = 'MIDI Learn ({0})'.format(last_cc_received)
            
        lines = [{
            "underline": True, 
            "text": "Edit MIDI CC mappaing"
            },
            justify_text('Param:', sound_parameters_info_dict[self.parameter_name][2]),  # Show parameter label instead of raw name
            justify_text('CC#:', cc_number),
            justify_text('Min:', '{0:.1f}'.format(self.min_range)),
            justify_text('Max:', '{0:.1f}'.format(self.max_range)),
        ]
        return frame_from_lines([self.get_default_header_line()] + lines)

    def on_encoder_rotated(self, direction, shift=False):
        self.cc_number += direction
        if self.cc_number < -1:
            self.cc_number = -1
        elif self.cc_number > 127:
            self.cc_number = 127

    def on_encoder_pressed(self, shift=False):
        # Save assignment and go back
        if self.cc_number > -1:
            cc_number = self.cc_number
        else:
            last_cc_received = int(sm.source_state.get(StateNames.LAST_CC_MIDI_RECEIVED, 0))
            if last_cc_received < 0:
                last_cc_received = 0
            cc_number = last_cc_received
        sm.send_osc_to_plugin('/add_or_update_cc_mapping', [self.sound_idx, int(self.random_id), cc_number, self.parameter_name, self.min_range, self.max_range])
        sm.show_global_message('Adding MIDI\nmapping...')
        sm.go_back()

    def on_fader_moved(self, fader_idx, value, shift=False):
        if fader_idx == 0:
            self.parameter_name = self.available_parameter_names[int(value * (len(self.available_parameter_names) - 1))]
        elif fader_idx == 1:
            self.cc_number = int(value * 128) - 1
        elif fader_idx == 2:
            self.min_range = value
        elif fader_idx == 3:
            self.max_range = value


class EnterNumberState(GoBackOnEncoderLongPressedStateMixin, State):

    value = 0
    minimum = 0
    maximum = 128
    title1 = "NoTitle1"
    title2 = "NoTitle2"
    callback = None  # Triggered when encoder pressed
    go_back_n_times = 0

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.value = kwargs.get('initial', 0)
        self.minimum = kwargs.get('minimum', 0)
        self.maximum = kwargs.get('maximum', 128)
        self.title1 = kwargs.get('title1', None)
        self.title2 = kwargs.get('title2', None)
        self.callback = kwargs.get('callback', None)
        self.go_back_n_times = kwargs.get('go_back_n_times', 0)

    def increase(self):
        self.value += 1
        if self.value > self.maximum:
            self.value = self.maximum

    def decrease(self):
        self.value -= 1
        if self.value < self.minimum:
            self.value = self.minimum

    def draw_display_frame(self):
        lines = [{
            "underline": True, 
            "text": self.title1
        }]
        if self.title2:
            lines += [{
                "underline": True, 
                "text": self.title1
            }]
        frame = frame_from_lines([self.get_default_header_line()] + lines)
        return add_centered_value_to_frame(frame, self.value)

    def on_encoder_rotated(self, direction, shift=False):
        if direction > 0:
            self.increase()
        else:
            self.decrease()

    def on_encoder_pressed(self, shift=False):
        if self.callback is not None:
            self.callback(self.value)
        sm.go_back(n_times=self.go_back_n_times)


class EnterNoteState(GoBackOnEncoderLongPressedStateMixin, State):

    title1 = "Title1"
    callback = None  # Triggered when encoder pressed
    go_back_n_times = 0

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.title1 = kwargs.get('title1', None)
        self.callback = kwargs.get('callback', None)
        self.go_back_n_times = kwargs.get('go_back_n_times', 0)

    def draw_display_frame(self):

        if sm.source_state.get(StateNames.MIDI_RECEIVED, False):
            last_note_received = sm.source_state.get(StateNames.LAST_NOTE_MIDI_RECEIVED, -1)
        else:
            last_note_received = -1

        if last_note_received != -1:
            sm.go_back(n_times=self.go_back_n_times)
            if self.callback is not None:
                self.callback(last_note_received)

        lines = [{
            "underline": True, 
            "text": self.title1
        }]
        frame = frame_from_lines([self.get_default_header_line()] + lines)
        return add_centered_value_to_frame(frame, "Enter a note...", font_size_big=False)

    def on_encoder_rotated(self, direction, shift=False):
        if direction > 0:
            self.increase()
        else:
            self.decrease()

    def on_encoder_pressed(self, shift=False):
        if self.callback is not None:
            self.callback(self.value)
        sm.go_back(n_times=self.go_back_n_times)


class EnterDataViaWebInterfaceState(GoBackOnEncoderLongPressedStateMixin, State):

    title = "NoTitle"
    callback = None  # Triggered when encoder pressed
    web_form_id = ""
    data_for_web_form_id = {}
    extra_data_for_callback = {}
    go_back_n_times = 0
    message = "Enter data on device..."
    
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.title = kwargs.get('title', "NoTitle")
        self.callback = kwargs.get('callback', None)
        self.web_form_id = kwargs.get('web_form_id', None)
        self.data_for_web_form_id = kwargs.get('data_for_web_form_id', None)
        self.extra_data_for_callback = kwargs.get('extra_data_for_callback', None)
        self.go_back_n_times = kwargs.get('go_back_n_times', 0)
        self.message = kwargs.get('message', 0)

    def draw_display_frame(self):
        lines = [{
            "underline": True, 
            "text": self.title
        }]
        frame = frame_from_lines([self.get_default_header_line()] + lines)
        return add_centered_value_to_frame(frame, self.message, font_size_big=False)

    def on_data_received(self, data):
        # data should be a dictionary here
        data.update(self.extra_data_for_callback)
        if self.callback is not None:
            self.callback(**data)
        sm.go_back(n_times=self.go_back_n_times)
        

class EnterTextViaHWOrWebInterfaceState(ShowHelpPagesMixin, EnterDataViaWebInterfaceState):

    max_length = 100
    current_text = []
    cursor_position = 0
    available_chars = [char for char in " abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"]
    char_position = 1
    store_recent = True
    frame_count = 0
    last_recent_loaded = 0
    help_pages = [[
        "B1 clear current",
        "B2 clear all",
        "B3 load random",
        "B4 load recent",
    ]]

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.store_recent = kwargs.get('store_recent', True)
        self.current_text = [char for char in kwargs.get('current_text', ' ')]
        self.current_text = self.current_text + [' '] * (self.max_length - len(self.current_text))

    def draw_display_frame(self):
        lines = [{
            "underline": True, 
            "text": self.title
        }]
        frame = frame_from_lines([self.get_default_header_line()] + lines)
        self.frame_count += 1
        chars_in_display = 9
        start_char = self.cursor_position - 4  # chars_in_display / 2
        end_char = self.cursor_position + 5  # chars_in_display / 2

        if start_char < 0 and end_char <= self.max_length:
            # We're at the beggning of the text, align it to the left
            start_char = 0
            end_char = start_char + chars_in_display
        elif start_char < self.max_length and end_char >= self.max_length:
            # We're at the end of the text, align it to the right
            end_char = self.max_length - 1
            start_char = end_char - chars_in_display
        elif 0 <= start_char < self.max_length and 0 <= end_char < self.max_length:
            # Normal case in which we're not in text edges
            pass

        return add_text_input_to_frame(
            frame, 
            text=self.current_text,
            cursor_position=self.cursor_position,
            blinking_state=self.frame_count % 2 == 0,
            start_char=start_char,
            end_char=end_char,
            )

    def on_encoder_rotated(self, direction, shift=False):
        if not shift:
            self.cursor_position += direction
            if self.cursor_position < 0:
                self.cursor_position = 0
            if self.cursor_position >= self.max_length:
                self.cursor_position = self.max_length - 1
            try:
                self.char_position = self.available_chars.index(self.current_text[self.cursor_position])
            except ValueError:
                self.char_position = 1
        else:
            self.char_position += direction
            self.current_text[self.cursor_position] = self.available_chars[self.char_position % len(self.available_chars)]

    def on_encoder_pressed(self, shift=False):
        entered_text = ''.join(self.current_text).strip()
        if self.store_recent:
            add_recent_query(entered_text)
        self.on_data_received({'query': entered_text})

    def on_data_received(self, data):
        if self.store_recent and 'query' in data:
            add_recent_query(data['query'])
        super().on_data_received(data)
        
    def on_button_pressed(self, button_idx, shift=False):
        if button_idx == 1:
            # Delete char in current cursor
            self.current_text[self.cursor_position] = ' '
        elif button_idx == 2:
            # Delete all chars
            self.current_text = [' '] * self.max_length
        elif button_idx == 3:
            # Load a random query from the prefined list
            random_query = random.choice(predefined_queries)
            self.current_text = [char for char in random_query] + [' '] * (self.max_length - len(random_query))
        elif button_idx == 4:
            # Load one of the recent stored queries (if any)
            recent_queries = get_recent_queries()
            if recent_queries:
                recent_query = recent_queries[self.last_recent_loaded % len(recent_queries)]
                self.current_text = [char for char in recent_query] + [' '] * (self.max_length - len(recent_query))
                self.last_recent_loaded += 1
        else:
            super().on_button_pressed(button_idx=button_idx, shift=shift)


class SoundSliceEditorState(ShowHelpPagesMixin, GoBackOnEncoderLongPressedStateMixin, State):

    def to_array(self, vorbis_file):
        bytes_per_sample = 2
        dtype = {
            1: numpy.int8,
            2: numpy.int16
        }
        # Convert the ctypes buffer to a NumPy array
        array = numpy.frombuffer(
            vorbis_file.buffer,
            dtype=dtype[bytes_per_sample]
        )
        # Reshape the array
        return array.reshape(
            (len(vorbis_file.buffer) // bytes_per_sample // vorbis_file.channels,
            vorbis_file.channels)
        )

    frame_count = 0
    sound_idx = -1
    sound_data_array = None  # numpy array of shape (x, 1)
    cursor_position = 0  # In samples
    sound_length = 0  # In samples
    sound_sr = 44100 # In samples
    min_zoom = 100  # In samples/pixel (the furthest away we can get)
    max_zoom = 1  # In samples/pixel (the closer we can get)
    current_zoom = min_zoom  # In samples/pixel
    display_width = DISPLAY_SIZE[0]  # In pixels
    half_display_width = display_width // 2  # In pixels
    scale = 1.0
    max_scale = 20.0
    min_scale = 0.2
    slices = []  # list slices to show (in samples)
    help_pages = [[
        "B1 set start",
        "B2 set end",
        "B3 set loop start",
        "B4 set loop end"
    ],[
        "B5 add slice",
        "B6 remove closest slice",
        "B7 remove all slices"
    ],[
        "F1 zoom",
        "F2 scale",
        "F3 cursor"
    ],[
        "E save and exit",
        "E+S save"
    ]]

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.sound_idx = kwargs.get('sound_idx', -1)
        if self.sound_idx > -1:
            sound_id = sm.gsp(self.sound_idx, StateNames.SOUND_ID, default=None)
            if sound_id is not None:
                if sm.gsp(self.sound_idx, StateNames.SOUND_LOCAL_FILE_PATH, default=''):
                    path = sm.gsp(self.sound_idx, StateNames.SOUND_LOCAL_FILE_PATH)
                else:
                    ty = sm.gsp(self.sound_idx, StateNames.SOUND_TYPE, default='')
                    if ty != '' and ty.lower() in ALLOWED_AUDIO_FILE_EXTENSIONS_IN_SLICE_EDITOR:
                        # If sound has type property, try with original file path first
                        original_filename = '{}.original.{}'.format(sound_id, ty)
                        path = os.path.join(sm.source_state.get(StateNames.SOUNDS_DATA_LOCATION, ''), original_filename)
                        if not os.path.exists(path):
                            # If original file does not exist, assign path to preview path
                            filename = '{}.ogg'.format(sound_id)
                            path = os.path.join(sm.source_state.get(StateNames.SOUNDS_DATA_LOCATION, ''), filename)
                    else:
                        # If sound has no "type" property, try directly with preview path
                        filename = '{}.ogg'.format(sound_id)
                        path = os.path.join(sm.source_state.get(StateNames.SOUNDS_DATA_LOCATION, ''), filename)
                try:
                    extension = path.split('.')[-1]
                except IndexError:
                    extension = ''
                if os.path.exists(path) and extension in ALLOWED_AUDIO_FILE_EXTENSIONS_IN_SLICE_EDITOR:
                    sm.show_global_message('Loading\nwaveform...', duration=10)
                    if extension == 'ogg':                
                        vorbis_file = pyogg.VorbisFile(path)
                        self.sound_sr = vorbis_file.frequency
                        self.sound_data_array = self.to_array(vorbis_file)[:, 0]  # Take 1 channel only
                    elif extension == 'wav':
                        self.sound_sr, self.sound_data_array = wavfile.read(path)    
                        if len(self.sound_data_array.shape) > 1:
                            self.sound_data_array = self.sound_data_array[:, 0]  # Take 1 channel only
                    elif extension == 'aiff':
                        aiff_file = aifc.open(path)
                        self.sound_sr = aiff_file.getframerate()
                        self.sound_data_array = numpy.fromstring(aiff_file.readframes(aiff_file.getnframes()), numpy.short).byteswap()
                        aiff_file.close()
                        if len(self.sound_data_array.shape) > 1:
                            self.sound_data_array = self.sound_data_array[:, 0]  # Take 1 channel only

                    self.sound_data_array = self.sound_data_array / abs(max(self.sound_data_array.min(), self.sound_data_array.max(), key=abs))  # Normalize
                    self.slices = [int(s * self.sound_sr) for s in sm.gsp(self.sound_idx, StateNames.SOUND_SLICES, default=[])]
                    self.sound_length = self.sound_data_array.shape[0]
                    self.min_zoom = (self.sound_length // self.display_width) + 1
                    self.current_zoom = self.min_zoom
                    audio_mean_value = numpy.absolute(self.sound_data_array).mean()
                    if audio_mean_value < 0.01:
                        # If the file contains very low energy, auto-scale it a bit
                        self.scale = 10
                    sm.show_global_message('', duration=0)
                else:
                    sm.show_global_message('File not found\nor not supported')
                        
    def draw_display_frame(self):
        self.frame_count += 1
        lines = [{
            "underline": True, 
            "move": True,
            "text": "S{0}:{1}".format(self.sound_idx + 1, sm.gsp(self.sound_idx, StateNames.SOUND_NAME))
        }]
        frame = frame_from_lines([self.get_default_header_line()] + lines)
        if self.sound_data_array is not None:
            start_sample = int(self.cursor_position - self.half_display_width * self.current_zoom)
            end_sample = int(self.cursor_position + self.half_display_width * self.current_zoom)
            n_samples_covered = end_sample - start_sample
            if start_sample < 0 and end_sample <= self.sound_length:
                # We're at the beggning of the sound, align it to the left
                start_sample = 0
                end_sample = start_sample + n_samples_covered
            elif start_sample < self.sound_length and end_sample >= self.sound_length:
                # We're at the end of the sound, align it to the right
                end_sample = self.sound_length - 1
                start_sample = end_sample - n_samples_covered
            elif 0 <= start_sample < self.sound_length and 0 <= end_sample < self.sound_length:
                # Normal case in which we're not in sound edges
                pass

            start_position = int(float(sm.gsparam(self.sound_idx, 'startPosition', default=0.0)) * self.sound_length)
            end_position = int(float(sm.gsparam(self.sound_idx, 'endPosition', default=1.0)) * self.sound_length)
            loop_start_position = int(float(sm.gsparam(self.sound_idx, 'loopStartPosition', default=0.0)) * self.sound_length)
            loop_end_position = int(float(sm.gsparam(self.sound_idx, 'loopEndPosition', default=1.0)) * self.sound_length)

            if self.current_zoom < 10:
                current_time_label = '{:.4f}s'.format(self.cursor_position / self.sound_sr)
            else:
                current_time_label = '{:.2f}s'.format(self.cursor_position / self.sound_sr)

            return add_sound_waveform_and_extras_to_frame(
                frame, 
                self.sound_data_array, 
                start_sample=start_sample, 
                end_sample=end_sample, 
                cursor_position=self.cursor_position, 
                scale=self.scale,
                slices=self.slices,
                start_position=start_position,
                end_position=end_position,
                loop_start_position=loop_start_position,
                loop_end_position=loop_end_position,
                current_time_label=current_time_label,
                blinking_state=self.frame_count % 2 == 0)
        else:
            return add_centered_value_to_frame(frame, "No sound loaded", font_size_big=False)

    def on_encoder_rotated(self, direction, shift=False):
        self.cursor_position += direction * self.current_zoom * (5 if not shift else 1)
        if self.cursor_position < 0:
            self.cursor_position = 0
        if self.cursor_position >= self.sound_data_array.shape[0]:
            self.cursor_position = self.sound_data_array.shape[0] - 1

    def on_encoder_pressed(self, shift):
        # Save slices
        time_slices = [s * 1.0 / self.sound_sr for s in self.slices]
        self.set_slices_for_sound(self.sound_idx, time_slices)
        sm.show_global_message('Updated {} slices'.format(len(time_slices)))
        if not shift:
            # If shift is not pressed, go back to sound selected state
            sm.go_back(n_times=2)

    def on_button_pressed(self, button_idx, shift=False):
        if button_idx == 1:
            # Set start position
            sm.send_osc_to_plugin('/set_sound_parameter', [self.sound_idx, "startPosition", self.cursor_position * 1.0 / self.sound_length])
        elif button_idx == 2:
            # Set loop start position
            sm.send_osc_to_plugin('/set_sound_parameter', [self.sound_idx, "loopStartPosition", self.cursor_position * 1.0 / self.sound_length])
        elif button_idx == 3:
            # Set loop end position
            sm.send_osc_to_plugin('/set_sound_parameter', [self.sound_idx, "loopEndPosition", self.cursor_position * 1.0 / self.sound_length])
        elif button_idx == 4:
            # Set end position
            sm.send_osc_to_plugin('/set_sound_parameter', [self.sound_idx, "endPosition", self.cursor_position * 1.0 / self.sound_length])
        elif button_idx == 5:
            # Add slice
            if self.cursor_position not in self.slices:
                self.slices = sorted(self.slices + [self.cursor_position])
        elif button_idx == 6:
            # Remove closest slice
            if self.slices:
                slice_to_remove_idx = min(range(len(self.slices)), key=lambda i: abs(self.slices[i] - self.cursor_position))
                self.slices = [s for i, s in enumerate(self.slices) if i != slice_to_remove_idx]
        elif button_idx == 7:
            # Remove all slices
            self.slices = [] 
        else:
            super().on_button_pressed(button_idx=button_idx, shift=shift)

    def on_fader_moved(self, fader_idx, value, shift=False):
        if fader_idx == 0:
            # Change zoom
            self.current_zoom = (((pow(value, 2) - 0) * (self.min_zoom - self.max_zoom)) / 1) + self.max_zoom
        elif fader_idx == 1:
            # Change scaling
            self.scale  = (((value - 0) * (self.max_scale - self.min_scale)) / 1) + self.min_scale
        elif fader_idx == 2:
            # Current position
            self.cursor_position = int(self.sound_length * value)
            if self.cursor_position < 0:
                self.cursor_position = 0
            if self.cursor_position >= self.sound_data_array.shape[0]:
                self.cursor_position = self.sound_data_array.shape[0] - 1


class SoundAssignedNotesEditorState(ShowHelpPagesMixin, GoBackOnEncoderLongPressedStateMixin, State):

    sound_idx = -1
    assigned_notes = []
    root_note = None
    cursor_position = 64  # In midi notes
    frame_count = 0
    range_start = None
    selecting_range_mode = False
    last_note_received_persistent = None
    hold_last_note = 1  # Show last played note for 1 second max
    last_note_updated_time = 0
    last_note_from_state = -1
    help_pages = [[
        "B1 toggle selected",
        "B1+S toggle last r",
        "B2 start range",
        "B3 end range",
    ],[
        "B4 clear all",
        "B5 set all",
        "B6 set root to sel",
        "B7 set root to last r",
    ],[
        "E save and exit",
        "E+S save",
        "EE se and u others",
        "EE+S s and u others",
    ]]

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.sound_idx = kwargs.get('sound_idx', -1)
        if self.sound_idx > -1:
            sound_assigned_notes = sm.gsp(self.sound_idx, StateNames.SOUND_ASSIGNED_NOTES, default=None)
            if sound_assigned_notes is not None:
                self.root_note = int(sm.gsparam(self.sound_idx, "midiRootNote", default=-1))
                self.assigned_notes = raw_assigned_notes_to_midi_assigned_notes(sound_assigned_notes)
                if self.assigned_notes:
                    if self.root_note > -1:
                        self.cursor_position = self.root_note
                    else:
                        self.cursor_position = self.assigned_notes[0]

    def save_assigned_notes(self, unassign_from_others=False):
        self.set_assigned_notes_for_sound(self.sound_idx, self.assigned_notes, root_note=self.root_note)
        if unassign_from_others:
            # Iterate over all sounds and make sure the notes for the current sound are not assinged
            # to any other sound
            for sound_idx, sound in enumerate(sm.source_state.get(StateNames.SOUNDS_INFO, [])):
                if sound_idx != self.sound_idx: 
                    sound_assigned_notes = sm.gsp(sound_idx, StateNames.SOUND_ASSIGNED_NOTES, default=None)
                    if sound_assigned_notes is not None:
                        midi_notes = raw_assigned_notes_to_midi_assigned_notes(sound_assigned_notes)
                        new_midi_notes = [note for note in midi_notes if note not in self.assigned_notes]
                        if len(midi_notes) != len(new_midi_notes):
                            self.set_assigned_notes_for_sound(sound_idx, new_midi_notes, root_note=int(sm.gsparam(sound_idx, "midiRootNote", default=-1)))

        sm.show_global_message('Updated {}\n assigned notes{}'.format(len(self.assigned_notes), ' (U)' if unassign_from_others else ''))
                    
    def draw_display_frame(self):
        lines = [{
            "underline": True, 
            "move": True,
            "text": "S{0}:{1}".format(self.sound_idx + 1, sm.gsp(self.sound_idx, StateNames.SOUND_NAME))
        }]
        frame = frame_from_lines([self.get_default_header_line()] + lines)
        self.frame_count += 1
        if self.range_start is not None:
            currently_selected_range = list(range(min(self.range_start, self.cursor_position), max(self.range_start, self.cursor_position) + 1))
        else:
            currently_selected_range = []

        current_last_note_from_state = sm.source_state.get(StateNames.LAST_NOTE_MIDI_RECEIVED, -1)
        if current_last_note_from_state != self.last_note_from_state or sm.source_state.get(StateNames.MIDI_RECEIVED, False):
            # A new note was triggered
            self.last_note_from_state = current_last_note_from_state
            self.last_note_updated_time = time.time()
        
        if time.time() - self.last_note_updated_time < self.hold_last_note:
            last_note_received = self.last_note_from_state
            self.last_note_received_persistent = last_note_received  # This is used for the "assign root note to last received" function
        else:
            last_note_received = -1
       
        return add_midi_keyboard_and_extras_to_frame(
            frame, 
            cursor_position=self.cursor_position, 
            assigned_notes=self.assigned_notes, 
            currently_selected_range=currently_selected_range, 
            last_note_received=last_note_received,
            blinking_state=self.frame_count % 2 == 0,
            root_note=self.root_note
            )

    def on_encoder_rotated(self, direction, shift=False):
        self.cursor_position += direction * (1 if not shift else 12)
        if self.cursor_position < 0:
            self.cursor_position = 0
        if self.cursor_position >= 128:
            self.cursor_position = 127

    def on_encoder_pressed(self, shift):
        # Save new assigned notes 
        self.save_assigned_notes(unassign_from_others=False)
        if not shift:
            # If shift is not pressed, go back to sound selected state
            sm.go_back(n_times=2)

    def on_encoder_double_pressed(self, shift):
        # Save new assigned notes and remove notes from other sounds
        self.save_assigned_notes(unassign_from_others=True)
        if not shift:
            # If shift is not pressed, go back to sound selected state
            sm.go_back(n_times=2)

    def on_button_pressed(self, button_idx, shift=False):
        if button_idx == 1:
            if not shift:
                # Toggle note in cursor position
                if self.cursor_position not in self.assigned_notes:
                    self.assigned_notes = sorted(self.assigned_notes + [self.cursor_position])
                else:
                    self.assigned_notes = [note for note in self.assigned_notes if note != self.cursor_position]
            else:
                # Toggle the last received note
                if self.last_note_received_persistent is not None:
                    if self.last_note_received_persistent not in self.assigned_notes:
                        self.assigned_notes = sorted(self.assigned_notes + [self.last_note_received_persistent])
                    else:
                        self.assigned_notes = [note for note in self.assigned_notes if note != self.last_note_received_persistent]
                    self.cursor_position = self.last_note_received_persistent  # Go to the one that was updated
        elif button_idx == 2:
            # Set start of range
            if not self.selecting_range_mode:
                # If not in range select mode, start range
                self.selecting_range_mode = True
                self.range_start = self.cursor_position
            else:
                # If already in range select mode, cancel current range
                self.selecting_range_mode = False
                self.range_start = None 
        elif button_idx == 3:
            # Set end of range
            if not self.selecting_range_mode:
                # If not in range select mode, do nothing
                pass
            else:
                if self.range_start is not None:
                    # If in range select mode, finish range
                    self.assigned_notes = sorted(list(set(self.assigned_notes + list(range(min(self.range_start, self.cursor_position), max(self.range_start, self.cursor_position) + 1)))))
                    self.range_start = None
                self.selecting_range_mode = False
        elif button_idx == 4:
            # Clear all assigned notes
            self.assigned_notes = []
        elif button_idx == 5:
            # Sett all assigned notes
            self.assigned_notes = list(range(0,128))
        elif button_idx == 6:
            # Set root note
            self.root_note = self.cursor_position
        elif button_idx == 7:
            # Set root note from last received midi note
            if self.last_note_received_persistent is not None:
                self.root_note = self.last_note_received_persistent
        else:
            super().on_button_pressed(button_idx=button_idx, shift=shift)


class FileChooserState(MenuCallbackState):

    base_path = ''

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.base_path = kwargs.get('base_path', '')
        self.shift_callback=lambda file_path: sm.send_osc_to_plugin('/play_sound_from_path', [file_path])  # Send preview sound OSC on shift+encoder
        self.go_back_n_times_shift_callback = 0
    
    def get_menu_item_lines(self):
        lines = []
        current_page = self.selected_item // self.page_size
        for item in self.items[current_page * self.page_size:(current_page + 1) * self.page_size]:
            lines.append({
                "invert": True if item == self.selected_item_name else False, 
                "move": True if item == self.selected_item_name else False,
                "text": item.replace(self.base_path, '').replace('/', '')
            })
        return lines

    def draw_display_frame(self):
        frame = super().draw_display_frame()
        if self.num_items > self.page_size:
            current_page = self.selected_item // self.page_size
            num_pages = int(math.ceil(len(self.items) // self.page_size))
            return add_scroll_bar_to_frame(frame, current_page, num_pages)
        else:
            return frame

    def on_encoder_rotated(self, direction, shift=False):
        super().on_encoder_rotated(direction, shift=shift)
        clear_moving_text_cache()
        sm.send_osc_to_plugin('/play_sound_from_path', [""])  # Stop sound being previewed (if any)

    def on_deactivating_state(self):
        super().on_deactivating_state()
        sm.send_osc_to_plugin('/play_sound_from_path', [""])  # Stop sound being previewed (if any)


class SoundChooserState(FileChooserState):

    base_path = ''
    sounds_data = {}

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.sounds_data = kwargs.get('sounds_data', '')
        self.shift_callback=lambda sound_name: sm.send_osc_to_plugin('/play_sound_from_path', [self.sounds_data.get(sound_name, {}).get("previews", {}).get("preview-lq-ogg", "")])  # Send preview sound OSC on shift+encoder



state_manager = StateManager()
sm = state_manager
state_manager.move_to(HomeState())
