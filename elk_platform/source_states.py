import math
import os
import random
import requests
import time

from freesound_api_key import FREESOUND_API_KEY

from helpers import justify_text, frame_from_lines, frame_from_start_animation, add_global_message_to_frame, START_ANIMATION_DURATION, translate_cc_license_url, StateNames, add_scroll_bar_to_frame, add_centered_value_to_frame

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
        indicators = "{0}{1}{2}{3}".format(
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
        sm.send_osc_to_plugin("/reapply_layout", [layout_type]) 
        sm.show_global_message("Updated layout")

    def set_num_voices(self, num_voices):
        sm.send_osc_to_plugin("/set_polyphony", [num_voices]) 

    def replace_sound_by_query(self, *args, **kwargs):
        sound_idx = kwargs.get('sound_idx', -1)
        if sound_idx >  -1:
            # TODO: refactor this to use proper Freesound python API client (?)
            sm.show_global_message("Finding sound...", duration=10)
            query = kwargs.get('query', "")
            min_length = float(kwargs.get('minSoundLength', '0'))
            max_length = float(kwargs.get('maxSoundLength', '300'))
            page_size = float(kwargs.get('pageSize', '50'))
            url = 'https://freesound.org/apiv2/search/text/?query={0}&filter=duration:[{1}+TO+{2}]&fields=id,previews,license,name,username&page_size={3}&token={4}'.format(query, min_length, max_length, page_size, FREESOUND_API_KEY)
            try:
                r = requests.get(url)
                response = r.json()
                if len(response['results']) > 0:
                    selected_sound = random.choice(response['results'])
                    sm.show_global_message("Replacing sound...")
                    sm.send_osc_to_plugin("/replace_sound_from_basic_info", [sound_idx, selected_sound['id'], selected_sound['name'], selected_sound['username'], selected_sound['license'], selected_sound['previews']['preview-hq-ogg']])
                else:
                    sm.show_global_message("No results found!")
            except Exception as e:
                print("ERROR while querying Freesound: {0}".format(e))
                sm.show_global_message("Error :(")

    def replace_sound_by_similarity(self, sound_idx, selected_sound_id):
        # TODO: refactor this to use proper Freesound python API client (?)
        sm.show_global_message("Finding sound...", duration=10)
        url = 'https://freesound.org/apiv2/sounds/{0}/similar?fields=id,previews,license,name,username&token={1}'.format(selected_sound_id, FREESOUND_API_KEY)
        try:
            r = requests.get(url)
            response = r.json()
            selected_sound = random.choice(response['results'])
            if len(response['results']) > 0:
                selected_sound = random.choice(response['results'])
                sm.show_global_message("Replacing sound...")
                sm.send_osc_to_plugin("/replace_sound_from_basic_info", [sound_idx, selected_sound['id'], selected_sound['name'], selected_sound['username'], selected_sound['license'], selected_sound['previews']['preview-hq-ogg']])
            else:
                sm.show_global_message("No results found!")
        except Exception as e:
            print("ERROR while querying Freesound: {0}".format(e))
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
                if shift and parameter_name == "pitch" or shift and parameter_name == "gain":
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


class MenuCallbackState(MenuState):

    callback = None
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
        self.go_back_n_times = kwargs.get('go_back_n_times', 0)

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

    def perform_action(self, action_name):
        if self.callback is not None:
            self.callback(self.selected_item)
        for i in range(0, self.go_back_n_times):
            sm.go_back()


class HomeContextualMenuState(GoBackOnEncoderLongPressedStateMixin, MenuState):

    OPTION_SAVE = "Save preset"
    OPTION_RELOAD = "Reload preset"
    OPTION_RELAYOUT = "Apply note layout..."
    OPTION_NUM_VOICES = "Set num voices..."
    OPTION_LOAD_PRESET = "Load preset..."

    name = "HomeContextualMenuState"
    items = [OPTION_SAVE, OPTION_RELOAD, OPTION_LOAD_PRESET, OPTION_RELAYOUT, OPTION_NUM_VOICES]
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
            sm.move_to(MenuCallbackState(items=['Contiguous', 'Interleaved'], selected_item=0, title1="Apply note layout...", callback=self.reapply_note_layout, go_back_n_times=2))
        elif action_name == self.OPTION_NUM_VOICES:
            current_num_voices = sm.source_state.get(StateNames.NUM_VOICES, 1)
            sm.move_to(EnterNumberState(initial=current_num_voices, minimum=1, maximum=32, title1="Num voices", callback=self.set_num_voices))
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
                callback=self.replace_sound_by_query, 
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
    OPTION_OPEN_IN_FREESOUND = "Open in Freesound"
    OPTION_DELETE = "Delete"

    name = "SoundSelectedContextualMenuState"
    sound_idx = -1
    items = [OPTION_REPLACE, OPTION_ASSIGNED_NOTES, OPTION_PRECISION_EDITOR, OPTION_OPEN_IN_FREESOUND, OPTION_DELETE]
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
            sm.show_global_message('Check browser')
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
        else:
            sm.show_global_message('Not implemented...')


class SoundParameterMIDILearnMenuState(State):

    name = "SoundParameterMIDILearnMenuState"


class EnterNumberState(State):

    value = 0
    minimum = 0
    maximum = 128
    title1 = "NoTitle1"
    title2 = "NoTitle2"
    callback = None
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
        sm.go_back()

    def on_encoder_long_pressed(self, shift=False):
        sm.go_back()


class EnterDataViaWebInterfaceState(State):

    title = "NoTitle"
    callback = None
    web_form_id = ""
    data_for_web_form_id = {}
    extra_data_for_callback = {}
    go_back_n_times = 0
    

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.title = kwargs.get('title', "NoTitle")
        self.callback = kwargs.get('callback', None)
        self.web_form_id = kwargs.get('web_form_id', None)
        self.data_for_web_form_id = kwargs.get('data_for_web_form_id', None)
        self.extra_data_for_callback = kwargs.get('extra_data_for_callback', None)
        self.go_back_n_times = kwargs.get('go_back_n_times', 0)

    def draw_display_frame(self):
        lines = [{
            "underline": True, 
            "text": self.title
        }]
        frame = frame_from_lines([self.get_default_header_line()] + lines)
        return add_centered_value_to_frame(frame, "Enter data on web...", font_size_big=False)

    def on_data_received(self, data):
        # data should be a dictionary here
        data.update(self.extra_data_for_callback)
        if self.callback is not None:
            self.callback(**data)
        for i in range(0, self.go_back_n_times):
            sm.go_back()

    def on_encoder_long_pressed(self, shift=False):
        sm.go_back()



state_manager = StateManager()
sm = state_manager
state_manager.move_to(HomeState())
