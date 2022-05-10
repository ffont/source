import time

from source_plugin_interface import SourcePluginInterface
from helpers import add_global_message_to_frame, clear_moving_text_cache
from elk_bridge import N_LEDS, N_FADERS
from .states_base import EnterDataViaWebInterfaceState, EnterTextViaHWOrWebInterfaceState
from .states_sound_selected import SoundSelectedState



def state_class_import(name):
    components = name.split('.')
    mod = __import__(components[0])
    for comp in components[1:]:
        mod = getattr(mod, comp)
    return mod


class UIStateManager(object):
    state_stack = []
    global_message = ('', 0, 0)  # (text, starttime, duration)
    ui_client = None
    frame_counter = 0
    open_url_in_browser = None
    should_show_start_animation = True
    block_ui_input = False
    waiting_to_go_to_last_loaded_sound = False

    def set_ui_client(self, ui_client):
        self.ui_client = ui_client

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

    def show_global_message(self, text, duration=1, only_if_current_message_matches=None):
        if only_if_current_message_matches is None:
            show_message = True
        else:
            if self.global_message is None:
                show_message = True
            else:
                show_message = only_if_current_message_matches in self.global_message[0]
        if show_message:
            self.global_message = (text, time.time(), duration)
        

    def move_to(self, new_state, replace_current=False):
        new_state.spi = spi
        new_state.sm = self
        clear_moving_text_cache()
        if self.state_stack:
            self.current_state.exit_help()
        if replace_current:
            self.current_state.on_deactivating_state()
            self.state_stack.pop()
        self.state_stack.append(new_state)
        self.current_state.on_activating_state()

    def move_to_selected_sound_state(self, sound_idx):
        # Special case of "move to" used to move to a "selected sound state" and which is needed to avoid some
        # circular dependency imports
        self.move_to(SoundSelectedState(sound_idx))

    def go_back(self, n_times=1):
        for i in range(0, n_times):
            if len(self.state_stack) > 1:
                self.current_state.on_deactivating_state()
                self.state_stack.pop()
                self.current_state.on_activating_state()

    def is_waiting_for_data_from_web(self):
        return type(self.current_state) == EnterDataViaWebInterfaceState or type(
            self.current_state) == EnterTextViaHWOrWebInterfaceState

    def process_data_from_web(self, data):
        if self.is_waiting_for_data_from_web():
            self.current_state.on_data_received(data)

    @property
    def current_state(self):
        return self.state_stack[-1]


state_manager = UIStateManager()
sm = state_manager

source_plugin_interface = SourcePluginInterface(sm)
spi = source_plugin_interface
