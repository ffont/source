import math
import os
import random
import time
import traceback

from helpers import justify_text, frame_from_lines, frame_from_start_animation, add_global_message_to_frame, START_ANIMATION_DURATION, translate_cc_license_url, StateNames, add_scroll_bar_to_frame, add_centered_value_to_frame

from freesound_interface import find_sound_by_similarity, find_sound_by_query, find_sounds_by_query, find_random_sounds

try:
    from elk_ui_custom import N_LEDS, N_FADERS
except ModuleNotFoundError:
    N_LEDS = 9


#  send_func (norm to val), get_func (val to val), parameter_label, value_label_template, set osc address
sound_parameters_info_dict = {
    "gain": (lambda x: 12.0 * 2.0 * (x - 0.5) if x >= 0.5 else 36.0 * 2.0 * (x - 0.5), lambda x: float(x), "Gain", "{0:.2f} dB", "/set_sound_parameter"),
    "pitch": (lambda x: 36.0 * 2 * (x - 0.5), lambda x: float(x), "Pitch", "{0:.2f}", "/set_sound_parameter"),
    "reverse": (lambda x: int(round(x)), lambda x: ['Off', 'On'][int(x)], "Reverse", "{0}", "/set_sound_parameter_int"),
    "launchMode": (lambda x: int(round(4 * x)), lambda x: ['Gate', 'Loop', 'Ping-pong', 'Trigger', 'Freeze'][int(x)], "Launch mode", "{0}", "/set_sound_parameter_int"),
    "startPosition": (lambda x: x, lambda x: float(x), "Start pos", "{0:.4f}", "/set_sound_parameter"),
    "endPosition": (lambda x: x, lambda x: float(x), "End pos", "{0:.4f}", "/set_sound_parameter"),
    "loopStartPosition": (lambda x: x, lambda x: float(x), "Loop st pos", "{0:.4f}", "/set_sound_parameter"),
    "loopEndPosition": (lambda x: x, lambda x: float(x), "Loop end pos", " {0:.4f}", "/set_sound_parameter"),
    "ampADSR.attack": (lambda x: 20.0 * pow(x, 2), lambda x: float(x), "A", "{0:.2f}s", "/set_sound_parameter"),
    "ampADSR.decay": (lambda x: 20.0 * pow(x, 2), lambda x: float(x), "D", "{0:.2f}s", "/set_sound_parameter"),
    "ampADSR.sustain": (lambda x: x, lambda x: float(x), "S", "{0:.2f}", "/set_sound_parameter"),
    "ampADSR.release": (lambda x: 20.0 * pow(x, 2), lambda x: float(x), "R", "{0:.2f}s", "/set_sound_parameter"),
    "filterCutoff": (lambda x: 10 + 20000 * pow(x, 2), lambda x: float(x), "Cutoff", "{0:.2f} Hz", "/set_sound_parameter"),
    "filterRessonance": (lambda x: x, lambda x: float(x), "Resso", "{0:.2f}", "/set_sound_parameter"),
    "filterKeyboardTracking": (lambda x: x, lambda x: float(x), "K.T.", "{0:.2f}", "/set_sound_parameter"),
    "filterADSR2CutoffAmt": (lambda x: 10.0 * x, lambda x: float(x), "Env amt", "{0:.2f}", "/set_sound_parameter"),    
    "filterADSR.attack": (lambda x: 20.0 * pow(x, 2), lambda x: float(x), "Filter A", "{0:.2f}s", "/set_sound_parameter"),
    "filterADSR.decay": (lambda x: 20.0 * pow(x, 2), lambda x: float(x), "Filter D", "{0:.2f}s", "/set_sound_parameter"),
    "filterADSR.sustain": (lambda x: x, lambda x: float(x), "Filter S", "{0:.2f}", "/set_sound_parameter"),
    "filterADSR.release": (lambda x: 20.0 * pow(x, 2), lambda x: float(x), "Filter R", "{0:.2f}s", "/set_sound_parameter"),
    "noteMappingMode": (lambda x: int(round(3 * x)), lambda x: ['Pitch', 'Slice', 'Both', 'Repeat'][int(x)], "Map mode", "{0}", "/set_sound_parameter_int"),
    "numSlices": (lambda x: int(round(32.0 * x)), lambda x: (['Auto onsets', 'Auto notes']+[str(x) for x in range(2, 101)])[int(x)], "# slices", "{0}", "/set_sound_parameter_int"),
    "playheadPosition": (lambda x: x, lambda x: float(x), "Playhead", "{0:.4f}", "/set_sound_parameter"),
    "freezePlayheadSpeed": (lambda x: 1 + 4999 * pow(x, 2), lambda x: float(x), "Freeze speed", "{0:.1f}", "/set_sound_parameter"),
    "pitchBendRangeUp": (lambda x: 36.0 * x, lambda x: float(x), "P.Bend down", "{0:.1f}", "/set_sound_parameter"),
    "pitchBendRangeDown": (lambda x: 36.0 * x, lambda x: float(x), "P.Bend up", "{0:.1f}", "/set_sound_parameter"),
    "mod2CutoffAmt": (lambda x: 100.0 * x, lambda x: float(x), "Mod2Cutoff", "{0:.2f}", "/set_sound_parameter"),
    "mod2GainAmt": (lambda x: 12.0 * 2 * (x - 0.5), lambda x: float(x), "Mod2Gain", "{0:.1f} dB", "/set_sound_parameter"),
    "mod2PitchAmt": (lambda x: 12.0 * 2 * (x - 0.5), lambda x: float(x), "Mod2Pitch", "{0:.2f} st", "/set_sound_parameter"),
    "vel2CutoffAmt": (lambda x: 10.0 * x, lambda x: float(x), "Vel2Cutoff", "{0:.1f}", "/set_sound_parameter"),
    "vel2GainAmt": (lambda x: x, lambda x: int(100 * float(x)), "Vel2Gain", "{0}%", "/set_sound_parameter"),
    "pan": (lambda x: 2.0 * (x - 0.5), lambda x: float(x), "Panning", "{0:.1f}", "/set_sound_parameter"),
}

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

EXTRA_PAGE_1_NAME = "extra1"
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
        None
    ]
]

midi_cc_available_parameters_list = ["startPosition", "endPosition", "loopStartPosition", "loopEndPosition", "playheadPosition", "freezePlayheadSpeed", "filterCutoff", "filterRessonance", "gain", "pan", "pitch"]

note_layout_types = ['Contiguous', 'Interleaved']

predefined_queries = ['wind', 'rain', 'piano', 'explosion', 'music', 'whoosh', 'woosh', 'intro', 'birds', 'footsteps', 'fire', '808', 'scream', 'water', 'bell', 'click', 'thunder', 'guitar', 'bass', 'beep', 'swoosh', 'pop', 'cartoon', 'magic', 'car', 'horror', 'vocal', 'game', 'trap', 'lofi', 'clap', 'happy', 'forest', 'ding', 'drum', 'kick', 'glitch', 'drop', 'transition', 'animal', 'gun', 'door', 'hit', 'punch', 'nature', 'jump', 'flute', 'sad', 'beat', 'christmas']
predefined_queries = sorted(predefined_queries)

class StateManager(object):

    state_stack = []
    global_message = ('', 0, 0)  # (text, starttime, duration)
    osc_client = None
    ui_client = None
    source_state = {}
    frame_counter = 0
    selected_open_sound_in_browser = None
    should_show_start_animation = True

    def set_osc_client(self, osc_client):
        self.osc_client = osc_client

    def set_ui_client(self, ui_client):
        self.ui_client = ui_client

    def update_source_state(self, source_state):
        self.source_state = source_state
        self.current_state.on_source_state_update()

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
            self.osc_client.send_message(address, values)

    def draw_display_frame(self):
        self.frame_counter += 1

        # Compute display frame of the current ui state and plugin state variables
        frame = self.current_state.draw_display_frame()

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
        if replace_current:
            self.current_state.on_deactivating_state()
            self.state_stack.pop()
        self.state_stack.append(new_state)
        self.current_state.on_activating_state()

    def go_back(self):
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
        return self.get_source_state_selected_sound_property(sound_idx, property_name, default)

    def is_waiting_for_data_from_web(self):
        return type(self.current_state) == EnterDataViaWebInterfaceState

    def process_data_from_web(self, data):
        if self.is_waiting_for_data_from_web():
            self.current_state.on_data_received(data)

    @property
    def current_state(self):
        return self.state_stack[-1]


class State(object):

    name = ""
    activation_time = None

    def __init__(self, *args, **kwargs):
        self.activation_time = time.time()

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

    def draw_display_frame(self):
        lines = [
            self.get_default_header_line(),
            {"underline": True, 
            "text": self.name},
            str(self.get_properties())
        ]
        return frame_from_lines(lines)

    def get_default_header_line(self):
        indicators = "{0}{1}{2}{3}{4}".format(
            "M" if sm.source_state.get(StateNames.MIDI_RECEIVED, False) else "",
            "!" if not sm.source_state[StateNames.NETWORK_IS_CONNECTED] else "", 
            "W" if sm.source_state.get(StateNames.IS_QUERYING_AND_DOWNLOADING, False) else "", 
            "*" if sm.source_state[StateNames.STATE_UPDATED_RECENTLY] else "", 
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
            sm.send_osc_to_plugin("/save_current_preset", [current_preset_name, current_preset_index])
            sm.show_global_message("Saving...")

    def load_preset(self, preset_idx):
        sm.send_osc_to_plugin("/load_preset", [preset_idx])
        sm.show_global_message("Loading {0}...".format(preset_idx), duration=5)

    def reload_current_preset(self):
        current_preset_index = sm.source_state.get(StateNames.LOADED_PRESET_INDEX, -1)
        if current_preset_index > -1:
            sm.send_osc_to_plugin("/load_preset", [current_preset_index])
            sm.show_global_message("Loading {0}...".format(current_preset_index), duration=5)

    def reapply_note_layout(self, layout_type):
        sm.send_osc_to_plugin("/reapply_layout", [['Contiguous', 'Interleaved'].index(layout_type)]) 
        sm.show_global_message("Updated layout")

    def set_num_voices(self, num_voices):
        sm.send_osc_to_plugin("/set_polyphony", [num_voices]) 

    def send_add_or_replace_sound_to_plugin(self, sound_idx, new_sound, assinged_notes="", trigger_download=""):
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
            ','.join(str(o) for o in sound_onsets_list),  # for use as slices
            assinged_notes,  # This should be string representation of hex number
            trigger_download  # "" -> yes, "none" -> no, "all" -> all sounds
            ])

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
                if note_layout_types[note_mapping_type] == 'Contiguous':
                    # In this case, all the notes mapped to this sound are contiguous in a range which depends on the total number of sounds to load
                    for i in range(sound_idx * n_notes_per_sound, (sound_idx + 1) * n_notes_per_sound):
                        midi_notes[i] = "1"
                else:
                    # Notes are mapped to sounds in interleaved fashion so each contiguous note corresponds to a different sound.
                    NOTE_MAPPING_INTERLEAVED_ROOT_NOTE = 36
                    root_note_for_sound = NOTE_MAPPING_INTERLEAVED_ROOT_NOTE + sound_idx
                    for i in range(root_note_for_sound, 128, n_sounds):
                        midi_notes[i] = "1"  # Map notes in upwards direction
                    for i in range(root_note_for_sound, 0, -n_sounds):
                        midi_notes[i] = "1"  # Map notes in downwards direction
                
                midi_notes = hex(int("".join(reversed(midi_notes)), 2))  # Convert to a format expected by send_add_or_replace_sound_to_plugin
            else:
                # If assigned_notes_per_sound_list is provided, take midi notes info from there
                midi_notes = assigned_notes_per_sound_list[sound_idx]

            self.send_add_or_replace_sound_to_plugin(-1, sound, assinged_notes=midi_notes, trigger_download="none" if sound_idx < n_sounds -1 else "all")
                    
    def add_or_replace_sound_by_query(self, *args, **kwargs):
        sound_idx = kwargs.get('sound_idx', -1)  # If no sound_idx kwarg is passed, then -1 will be used (which means to add a new sound)
        query = kwargs.get('query', "")
        sm.show_global_message("Searching {}...".format(query), duration=3600)
        min_length = float(kwargs.get('minSoundLength', '0'))
        max_length = float(kwargs.get('maxSoundLength', '300'))
        page_size = float(kwargs.get('pageSize', '50'))
        try:
            selected_sound = find_sound_by_query(query=query, min_length=min_length, max_length=max_length, page_size=page_size)
            if selected_sound is not None:
                sm.show_global_message("Loading sound...")
                self.send_add_or_replace_sound_to_plugin(sound_idx, selected_sound)
            else:
                sm.show_global_message("No results found!")
        except Exception as e:
            print("ERROR while querying Freesound: {0}".format(e))
            traceback.print_tb(e.__traceback__)
            sm.show_global_message("Error :(")

    def replace_sound_by_similarity(self, sound_idx, selected_sound_id):
        sm.show_global_message("Searching similar...", duration=3600)
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

    def new_preset_by_query(self, *args, **kwargs):
        query = kwargs.get('query', "")
        sm.show_global_message("Searching\n{}...".format(query), duration=3600)
        min_length = float(kwargs.get('minSoundLength', '0'))
        max_length = float(kwargs.get('maxSoundLength', '10'))
        page_size = int(kwargs.get('pageSize', '150'))
        n_sounds = int(kwargs.get('numSounds', '16'))
        note_mapping_type = int(kwargs.get('noteMappingType', '1'))
        try:
            new_sounds = find_sounds_by_query(query=query, n_sounds=n_sounds, min_length=min_length, max_length=max_length, page_size=page_size)
            if not new_sounds:
                sm.show_global_message("No results found!")
            else:
                sm.show_global_message("Loading sounds...")
                self.load_query_results(new_sounds, note_mapping_type)
                
        except Exception as e:
            print("ERROR while querying Freesound: {0}".format(e))
            traceback.print_tb(e.__traceback__)
            sm.show_global_message("Error :(")

    def new_preset_from_predefined_query(self, query, **kwargs):
        kwargs.update({"query": query})
        self.new_preset_by_query(**kwargs)  

    def new_preset_by_random_sounds(self, *args, **kwargs):

        def show_progress_message(count, total):
            sm.show_global_message("Entering the\nunknown... [{}/{}]".format(count + 1, total), duration=3600)

        sm.show_global_message("Entering the\nunknown...", duration=3600)
        min_length = float(kwargs.get('minSoundLength', '0'))
        max_length = float(kwargs.get('maxSoundLength', '10'))
        n_sounds = int(kwargs.get('numSounds', '16'))
        note_mapping_type = int(kwargs.get('noteMappingType', '1'))
        try:
            new_sounds = find_random_sounds(n_sounds=n_sounds, min_length=min_length, max_length=max_length, report_callback=show_progress_message)
            if not new_sounds:
                sm.show_global_message("No results found!")
            else:
                sm.show_global_message("Loading sounds...")
                self.load_query_results(new_sounds, note_mapping_type)

        except Exception as e:
            print("ERROR while querying Freesound: {0}".format(e))
            traceback.print_tb(e.__traceback__)
            sm.show_global_message("Error :(")

    def new_preset_by_similar_sounds(self, note_mapping_type=1):

        def show_progress_message(count, total):
            sm.show_global_message("Searching similar\nsounds... [{}/{}]".format(count + 1, total), duration=3600)

        sm.show_global_message("Searching similar\nsounds...", duration=3600)
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

    def set_sound_params_from_precision_editor(self, *args, **kwargs):
        sound_idx = kwargs.get('sound_idx', -1)
        if sound_idx >  -1:
            duration = float(kwargs['duration'])
            for parameter_name in ['startPosition', 'endPosition', 'loopStartPosition', 'loopEndPosition']:
                sm.send_osc_to_plugin('/set_sound_parameter', [sound_idx, parameter_name, float(kwargs[parameter_name])/duration])
            
            slices = sorted([val for key, val in kwargs.items() if key.startswith("slice_")])
            sm.send_osc_to_plugin('/set_slices', [sound_idx, ','.join([str(s) for s in slices])])            

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
        sm.go_back()


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

    name = "HomeState"
    pages = sound_parameter_pages + [EXTRA_PAGE_1_NAME]

    def draw_display_frame(self):
        lines = [{
            "underline": True, 
            "text":  "{0} sounds".format(sm.source_state.get(StateNames.NUM_SOUNDS, 0)),
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
        else:
            # Show page parameter values
            for parameter_name in self.current_page_data:
                if parameter_name is not None:
                    _, get_func, parameter_label, value_label_template, _ = sound_parameters_info_dict[parameter_name]
                    
                    # Check if all loaded sounds have the same value for that parameter. If that is the case, show the value, otherwise don't show any value
                    all_sounds_values = []
                    last_value = None
                    for sound_idx in range(0, sm.source_state.get(StateNames.NUM_SOUNDS, 0)):
                        processed_val = get_func(sm.gsp(sound_idx, StateNames.SOUND_PARAMETERS).get(parameter_name, 0))
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

        return self.draw_scroll_bar(frame_from_lines([self.get_default_header_line()] + lines))

    def on_activating_state(self):
        sm.unset_all_leds()
        sm.set_all_fader_leds()

    def on_deactivating_state(self):
        sm.unset_all_fader_leds()

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

    def on_encoder_long_pressed(self, shift=False):
        # Save the preset in its location
        self.save_current_preset()

    def on_fader_moved(self, fader_idx, value, shift=False):
        if self.current_page_data == EXTRA_PAGE_1_NAME:
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


class SoundSelectedState(ChangePresetOnEncoderShiftRotatedStateMixin, GoBackOnEncoderLongPressedStateMixin, PaginatedState):

    name = "SoundSelectedState"
    pages = sound_parameter_pages + [EXTRA_PAGE_1_NAME]
    sound_idx = -1
    selected_sound_is_playing = False

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
        lines = [{
            "underline": True, 
            "text": "S{0}:{1}".format(self.sound_idx, sm.gsp(self.sound_idx, StateNames.SOUND_NAME))
        }]

        if self.current_page_data == EXTRA_PAGE_1_NAME:
            # Show some sound information
            sound_download_progress = sm.gsp(self.sound_idx, StateNames.SOUND_DOWNLOAD_PROGRESS)
            lines += [
                justify_text('ID:', '{0}'.format(sm.gsp(self.sound_idx, StateNames.SOUND_ID))),
                justify_text('User:', '{0}'.format(sm.gsp(self.sound_idx, StateNames.SOUND_AUTHOR))),
                justify_text('CC License:', '{0}'.format(sm.gsp(self.sound_idx, StateNames.SOUND_LICENSE))),
                justify_text('Duration:', '{0:.2f}s'.format(sm.gsp(self.sound_idx, StateNames.SOUND_DURATION))) if sound_download_progress == '100' \
                    else justify_text('Downloading...', '{0}%'.format(sound_download_progress)),
            ]
        else:
            # Show page parameter values
            for parameter_name in self.current_page_data:
                if parameter_name is not None:
                    _, get_func, parameter_label, value_label_template, _ = sound_parameters_info_dict[parameter_name]
                    sound_parameters = sm.gsp(self.sound_idx, StateNames.SOUND_PARAMETERS)
                    if (type(sound_parameters) == dict) and parameter_name in sound_parameters:
                        state_val = sound_parameters[parameter_name]
                        parameter_value_label = value_label_template.format(get_func(state_val))
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
    
    def on_button_pressed(self, button_idx, shift=False):
        # Stop current sound
        self.stop_selected_sound()
        
        # Select another sound
        sound_idx = self.get_sound_idx_from_buttons(button_idx, shift=shift)
        if sound_idx > -1:
            sm.move_to(SoundSelectedState(sound_idx, current_page=self.current_page), replace_current=True)

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
            sm.move_to(SoundSelectedContextualMenuState(sound_idx=self.sound_idx))
        else:
            # Trigger the selected sound
            self.play_selected_sound()

    def on_fader_moved(self, fader_idx, value, shift=False):
        if self.current_page_data == EXTRA_PAGE_1_NAME:
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
        else:
            self.previous_item()

    def on_encoder_pressed(self, shift=False):
        self.perform_action(self.selected_item_name)

    def perform_action(self, action_name):
        pass


class MenuCallbackState(GoBackOnEncoderLongPressedStateMixin, MenuState):

    callback = None  # Triggered when encoder pressed
    shift_callback = None  # Triggered when shift+encoder pressed
    update_items_callback = None  # Callback to update available items when state is updated
    extra_data_for_callback = {}
    title1 = "NoTitle1"
    title2 = "NoTitle2"
    page_size = 4
    go_back_n_times = 0

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
            else:
                if shift:
                    self.shift_callback(self.selected_item_name)
                else:
                    if self.extra_data_for_callback is None:
                        self.callback(self.selected_item_name)
                    else:
                        self.callback(self.selected_item_name, **self.extra_data_for_callback)
        for i in range(0, self.go_back_n_times):
            sm.go_back()

    def on_source_state_update(self):
        if self.update_items_callback is not None:
            self.items = self.update_items_callback()
            if self.selected_item >= len(self.items):
                self.selected_item = len(self.items) - 1


class ReverbSettingsMenuState(GoBackOnEncoderLongPressedStateMixin, PaginatedState):

    name = "ReverbSettingsMenuState"
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


class NewPresetDetailedSettingsState(GoBackOnEncoderLongPressedStateMixin, State):

    num_sounds = 16
    min_length = 0.0
    max_length = 0.5
    layout = note_layout_types[1]
    callback = None
    go_back_n_times = 0

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.num_sounds = kwargs.get('num_sounds', 16) 
        self.min_length = kwargs.get('min_length', 0.0)
        self.max_length = kwargs.get('max_length', 0.5)
        self.layout = kwargs.get('layout', note_layout_types[1])
        self.callback = kwargs.get('callback', None)
        self.go_back_n_times = kwargs.get('go_back_n_times', 0)

    def draw_display_frame(self):    
        lines = [{
            "underline": True, 
            "text": "New sounds settings..."
            },
            justify_text('Num sounds:', '{0}'.format(self.num_sounds)),  # Show parameter label instead of raw name
            justify_text('Min length:', '{0:.1f}s'.format(self.min_length)),
            justify_text('Max length:', '{0:.1f}s'.format(self.max_length)),
            justify_text('Layout:', self.layout),
            
        ]
        return frame_from_lines([self.get_default_header_line()] + lines)

    def on_encoder_rotated(self, direction, shift=False):
        self.num_sounds += direction
        if self.num_sounds < 1:
            self.num_sounds = 1
        elif self.num_sounds > 128:
            self.num_sounds = 128

    def on_encoder_pressed(self, shift=False):
        # Move to query chooser state
        if self.callback is not None:
            self.callback(
                num_sounds=self.num_sounds,
                min_length=self.min_length,
                max_length=self.max_length,
                layout=self.layout
            )
        for i in range(0, self.go_back_n_times):
            sm.go_back()

    def on_fader_moved(self, fader_idx, value, shift=False):
        if fader_idx == 0:
            self.num_sounds = int(value * 128)
        elif fader_idx == 1:
            self.min_length = float(pow(value, 2) * 300)
        elif fader_idx == 2:
            self.max_length = float(pow(value, 2) * 300)
        elif fader_idx == 3:
            self.layout = note_layout_types[int(value * (len(note_layout_types) - 1))]


class NewPresetOptionsMenuState(GoBackOnEncoderLongPressedStateMixin, MenuState):

    OPTION_BY_QUERY = "From new query"
    OPTION_BY_PREDEFINED_QUERY = "From predefined query"
    OPTION_BY_SIMILARITY = "By sound similarity"
    OPTION_RANDOM = "Random sounds"
    
    name = "NewPresetOptionsMenuState"
    items = [OPTION_BY_QUERY, OPTION_BY_PREDEFINED_QUERY, OPTION_BY_SIMILARITY, OPTION_RANDOM]
    page_size = 4

    def draw_display_frame(self):
        lines = [{
            "underline": True, 
            "text": "Get new sounds..."
        }]
        lines += self.get_menu_item_lines()
        return frame_from_lines([self.get_default_header_line()] + lines)

    def move_to_enter_query_on_device_menu(self, num_sounds, min_length, max_length, layout):
        sm.move_to(EnterDataViaWebInterfaceState(
                title="New query", 
                web_form_id="enterQuery", 
                callback=self.new_preset_by_query, 
                extra_data_for_callback={
                    'minSoundLength': min_length,
                    'maxSoundLength': max_length,
                    'numSounds': num_sounds, 
                    'noteMappingType': note_layout_types.index(layout)
                },
                message="Enter query\non device...",
                go_back_n_times=4)) 
    
    def move_to_select_predefined_query_menu(self, num_sounds, min_length, max_length, layout):
        sm.move_to(MenuCallbackState(
                items=predefined_queries, 
                selected_item=random.choice(range(0, len(predefined_queries))), 
                title1="Predefined query",
                callback=self.new_preset_from_predefined_query, 
                extra_data_for_callback={
                    'minSoundLength': min_length,
                    'maxSoundLength': max_length,
                    'numSounds': num_sounds, 
                    'noteMappingType': note_layout_types.index(layout)
                },
                go_back_n_times=4
                ))

    def perform_action(self, action_name):
        if action_name == self.OPTION_BY_QUERY:
            sm.move_to(NewPresetDetailedSettingsState(
                callback=self.move_to_enter_query_on_device_menu,
                go_back_n_times=0
            ))
        elif action_name == self.OPTION_BY_PREDEFINED_QUERY:
            sm.move_to(NewPresetDetailedSettingsState(
                callback=self.move_to_select_predefined_query_menu,
                go_back_n_times=0
            ))
        elif action_name == self.OPTION_RANDOM:
            sm.move_to(NewPresetDetailedSettingsState(
                callback=self.new_preset_by_random_sounds,
                go_back_n_times=3
            ))
        elif action_name == self.OPTION_BY_SIMILARITY:
            current_note_mapping_type = sm.source_state.get(StateNames.NOTE_LAYOUT_TYPE, 1)
            self.new_preset_by_similar_sounds(note_mapping_type=current_note_mapping_type)
            sm.go_back()
            sm.go_back()
            sm.go_back()
        else:
            sm.show_global_message('Not implemented...')


class HomeContextualMenuState(GoBackOnEncoderLongPressedStateMixin, MenuState):

    OPTION_SAVE = "Save preset"
    OPTION_RELOAD = "Reload preset"
    OPTION_NEW_SOUNDS = "Get new sounds..."
    OPTION_ADD_NEW_SOUND = "Add new sound..."
    OPTION_REVERB = "Reverb settings..."
    OPTION_RELAYOUT = "Apply note layout..."
    OPTION_NUM_VOICES = "Set num voices..."
    OPTION_LOAD_PRESET = "Load preset..."

    name = "HomeContextualMenuState"
    items = [OPTION_SAVE, OPTION_RELOAD, OPTION_LOAD_PRESET, OPTION_NEW_SOUNDS, OPTION_ADD_NEW_SOUND, OPTION_RELAYOUT, OPTION_REVERB, OPTION_NUM_VOICES]
    page_size = 5

    def draw_display_frame(self):
        lines = self.get_menu_item_lines()
        return frame_from_lines([self.get_default_header_line()] + lines)

    def perform_action(self, action_name):
        if action_name == self.OPTION_SAVE:
            self.save_current_preset()
            sm.go_back()
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

            current_preset_index = sm.source_state.get(StateNames.LOADED_PRESET_INDEX, 0)
            if not preset_names:
                sm.move_to(EnterNumberState(initial=current_preset_index, minimum=0, maximum=127, title1="Load preset...", callback=self.load_preset, go_back_n_times=2))
            else:
                sm.move_to(MenuCallbackState(items=['{0}:{1}'.format(i, preset_names.get(i, 'empty')) for i in range(0, 128)], selected_item=current_preset_index, title1="Load preset...", callback=self.load_preset, go_back_n_times=2))
        elif action_name == self.OPTION_REVERB:
            sm.move_to(ReverbSettingsMenuState())
        elif action_name == self.OPTION_ADD_NEW_SOUND:
            sm.move_to(EnterDataViaWebInterfaceState(
                title="Add new sound...", 
                web_form_id="replaceSound", 
                extra_data_for_callback={}, 
                callback=self.add_or_replace_sound_by_query, 
                go_back_n_times=2))
        elif action_name == self.OPTION_NEW_SOUNDS:
            sm.move_to(NewPresetOptionsMenuState())
        else:
            sm.show_global_message('Not implemented...')


class ReplaceByOptionsMenuState(GoBackOnEncoderLongPressedStateMixin, MenuState):

    OPTION_BY_QUERY = "New query"
    OPTION_BY_SIMILARITY = "Find similar"
    OPTION_FROM_DISK = "Load from disk"

    name = "ReplaceByOptionsMenuState"
    sound_idx = -1
    items = [OPTION_BY_QUERY, OPTION_BY_SIMILARITY, OPTION_FROM_DISK]
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
        lines = [{
            "underline": True, 
            "text": "S{0}:{1}".format(self.sound_idx, sm.gsp(self.sound_idx, StateNames.SOUND_NAME))
        }, {
            "underline": True, 
            "text": "Replace by..."
        }]
        lines += self.get_menu_item_lines()
        return frame_from_lines([self.get_default_header_line()] + lines)

    def perform_action(self, action_name):
        if action_name == self.OPTION_BY_QUERY:
            sm.move_to(EnterDataViaWebInterfaceState(
                title="Replace sound by", 
                web_form_id="replaceSound", 
                extra_data_for_callback={'sound_idx': self.sound_idx}, 
                callback=self.add_or_replace_sound_by_query, 
                go_back_n_times=3))
        elif action_name == self.OPTION_BY_SIMILARITY:
            selected_sound_id = sm.gsp(self.sound_idx, StateNames.SOUND_ID)
            if selected_sound_id != '-':
                self.replace_sound_by_similarity(self.sound_idx, selected_sound_id)
                sm.go_back()
                sm.go_back()  # Go back 2 times because option is 2-levels deep in menu hierarchy
        else:
            sm.show_global_message('Not implemented...')



class SoundSelectedContextualMenuState(GoBackOnEncoderLongPressedStateMixin, MenuState):

    OPTION_REPLACE = "Replace by..."
    OPTION_ASSIGNED_NOTES = "Assigned notes..."
    OPTION_PRECISION_EDITOR = "Precision editor..."
    OPTION_MIDI_CC = "MIDI CC mappings..."
    OPTION_OPEN_IN_FREESOUND = "Open in Freesound"
    OPTION_DELETE = "Delete"

    MIDI_CC_ADD_NEW_TEXT = "Add new..."

    name = "SoundSelectedContextualMenuState"
    sound_idx = -1
    items = [OPTION_REPLACE, OPTION_MIDI_CC, OPTION_ASSIGNED_NOTES, OPTION_PRECISION_EDITOR, OPTION_OPEN_IN_FREESOUND, OPTION_DELETE]
    page_size = 4

    def __init__(self, sound_idx, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.sound_idx = sound_idx

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
            sm.move_to(SoundSelectSoundSelectedContextualMenuStateedState(num_sounds -1), replace_current=True)
        elif self.sound_idx < 0 and num_sounds > 0:
            sm.move_to(SoundSelectedContextualMenuState(0), replace_current=True)

    def draw_display_frame(self):
        lines = [{
            "underline": True, 
            "text": "S{0}:{1}".format(self.sound_idx, sm.gsp(self.sound_idx, StateNames.SOUND_NAME))
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
                sm.selected_open_sound_in_browser = selected_sound_id
            sm.go_back()
        elif action_name == self.OPTION_PRECISION_EDITOR:
            selected_sound_id = sm.gsp(self.sound_idx, StateNames.SOUND_ID)
            sound_parameters = sm.gsp(self.sound_idx, StateNames.SOUND_PARAMETERS, default={})
            sm.move_to(EnterDataViaWebInterfaceState(
                title="Precision editor", 
                web_form_id="soundEditor", 
                data_for_web_form_id={
                    'soundIdx': self.sound_idx, 
                    'soundID': selected_sound_id, 
                    'soundName': sm.gsp(self.sound_idx, StateNames.SOUND_NAME),
                    'soundOGGURL': sm.gsp(self.sound_idx, StateNames.SOUND_OGG_URL),
                    'soundPath': os.path.join(sm.source_state.get(StateNames.SOUNDS_DATA_LOCATION, ''), '{0}.ogg'.format(selected_sound_id)),
                    'startPosition': float(sound_parameters.get('startPosition', 0)),
                    'endPosition': float(sound_parameters.get('endPosition', 1)),
                    'loopStartPosition': float(sound_parameters.get('loopStartPosition', 0)),
                    'loopEndPosition': float(sound_parameters.get('loopEndPosition', 1)),
                    'slices': sm.gsp(self.sound_idx, StateNames.SOUND_SLICES),
                    },
                extra_data_for_callback={'sound_idx': self.sound_idx}, 
                callback=self.set_sound_params_from_precision_editor, 
                go_back_n_times=2))
        elif action_name == self.OPTION_MIDI_CC:
            sm.move_to(
                MenuCallbackState(
                items=self.get_midi_cc_items_for_midi_cc_assignments_menu(), 
                selected_item=0, 
                title1="MIDI CC mappings......", 
                callback=self.handle_select_midi_cc_assignment, 
                shift_callback=self.handle_delete_midi_cc_assignment,
                update_items_callback=self.get_midi_cc_items_for_midi_cc_assignments_menu,
                go_back_n_times=0)  # Don't go back because we go into another menu and we handle this individually in the callbacks of the inside options
            )
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
        for i in range(0, self.go_back_n_times):
            sm.go_back()


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
        for i in range(0, self.go_back_n_times):
            sm.go_back()



state_manager = StateManager()
sm = state_manager
state_manager.move_to(HomeState())
