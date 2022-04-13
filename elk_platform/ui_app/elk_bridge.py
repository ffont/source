import sys
import time
from collections import defaultdict

from system_stats import system_stats
from helpers import get_platform, StateNames, Timer, START_ANIMATION_DURATION, frame_from_start_animation
try:
    from elkpy import sushicontroller as sc
    from elkpy import midicontroller as mc
except ImportError:
    # Not running in ELK platform, no problem as ELK-specific code won't be run
    pass
try:
    from elk_ui_custom import ElkUIController, N_LEDS
except ModuleNotFoundError:
    # Not running in ELK platform, no problem as ELK-specific code won't be run
    N_LEDS = 9


SOURCE_TRACK_ID = 0
BUTTON_LONG_PRESS_TIME = 0.25
BUTTON_DOUBLE_PRESS_TIME = 0.2


class ElkBridge(object):

    buttons_state = {}
    button_pressing_log = defaultdict(list)
    button_timers = [None, None, None, None, None, None, None, None, None]

    encoder_pressing_log = []
    encoder_timer = None

    buttons_sent_note_on = {}
    selected_sound_idx = None
    current_parameter_page = 0
    starttime = 0

    uic = None
    sushic = None
    midic = None

    ui_state_manager = None

    current_frame = None

    def __init__(self, ui_state_manager, source_plugin_interface, refresh_fps):
        self.ui_state_manager = ui_state_manager
        self.source_plugin_interface = source_plugin_interface
        self.refresh_fps = refresh_fps
        while self.sushic is None:
            try:
                self.sushic = sc.SushiController()
                # Print available parameters
                for track_info in self.sushic.audio_graph.get_all_tracks():
                    print('Track ID {}'.format(track_info.id))
                    print(self.sushic.parameters.get_track_parameters(track_info.id))
                sys.stdout.flush()

                # Set init gain to high number
                controller_id = SOURCE_TRACK_ID
                param_id = 0  # Gain
                self.sushic.parameters.set_parameter_value(controller_id, param_id, 0.92)
                print("Initialized SushiController")
            except Exception as e:
                print('ERROR initializing SushiController: {0}'.format(e))
                if get_platform() == "ELK":
                    time.sleep(1)  # Wait for 1 second and try again
                else:
                    break  # Break while because outside ELK platform this will never succeed

        while self.midic is None:
            try:
                self.midic = mc.MidiController()
                midic_initialized = True
                print("Initialized MidiController")
                self.connect_midi_channels_to_source()
            except Exception as e:
                print('ERROR initializing MidiController: {0}'.format(e))
                if get_platform() == "ELK":
                    time.sleep(1)  # Wait for 1 second and try again
                else:
                    break  # Break while because outside ELK platform this will never succeed

        while self.uic is None:
            try:
                self.uic = ElkUIController(self.handle_faders,
                                           self.handle_buttons,
                                           self.handle_encoder_button,
                                           self.handle_encoder,
                                           self.handle_potentiometer)
                self.ui_state_manager.set_ui_client(self.uic)
                print("Initialized ElkUIController")
            except Exception as e:
                print('ERROR initializing ElkUIController: {0}'.format(e))
                if get_platform() == "ELK":
                    time.sleep(1)  # Wait for 1 second and try again
                else:
                    break  # Break while because outside ELK platform this will never succeed

        # Compute first display frame
        self.compute_display_frame(0)

    def connect_midi_channels_to_source(self, midi_channel=0):
        if self.midic is not None:
            print("Connecting midi channel {} to source (0=omni)".format(midi_channel))

            raw_midi = True
            track_identifier = 0  # Source track ID
            port = 0

            # First disconnect all existing connections (if any)
            for c in self.midic.get_all_kbd_input_connections():
                self.midic.disconnect_kbd_input(c.track, c.channel, c.port, c.raw_midi)

            # Now make new connection
            # channel 0 = omni, channels 1-16 are normal midi channels
            self.midic.connect_kbd_input_to_track(self.midic._sushi_proto.TrackIdentifier(id=track_identifier),
                                                  self.midic._sushi_proto.MidiChannel(channel=midi_channel), port, raw_midi)
            self.current_midi_in_channel = midi_channel

    def is_shift_pressed(self):
        return self.buttons_state.get(0, 0) == 1

    def handle_potentiometer(self, val):
        if self.sushic is not None:
            controller_id = SOURCE_TRACK_ID
            param_id = 0  # Gain
            # For some reason the gain only seems to be doing something when value is set between
            # 0.5 and 1.0, so we adapt the range
            val = val/2 + 0.5
            self.sushic.parameters.set_parameter_value(controller_id, param_id, val)

    def handle_faders(self, fader_idx, val):
        if not self.ui_state_manager.block_ui_input:
            self.ui_state_manager.current_state.on_fader_moved(fader_idx, val, shift=self.is_shift_pressed())

    def handle_buttons(self, button_idx, val):

        def delayed_double_press_button_check(button_idx):
            last_time_pressed = self.button_pressing_log[button_idx][-1]
            try:
                previous_time_pressed = self.button_pressing_log[button_idx][-2]
            except IndexError:
                previous_time_pressed = 0
            if last_time_pressed - previous_time_pressed < BUTTON_DOUBLE_PRESS_TIME:
                # If time between last 2 pressings is shorter than BUTTON_DOUBLE_PRESS_TIME, trigger double press action
                if not self.ui_state_manager.block_ui_input:
                    self.ui_state_manager.current_state.on_button_double_pressed(button_idx, shift=self.is_shift_pressed())
            else:
                if not self.ui_state_manager.block_ui_input:
                    self.ui_state_manager.current_state.on_button_pressed(button_idx, shift=self.is_shift_pressed())

        is_being_pressed = int(val) == 1
        if is_being_pressed:
            # Trigger the raw press action
            if not self.ui_state_manager.block_ui_input:
                self.ui_state_manager.current_state.on_button_down(button_idx, shift=self.is_shift_pressed())

            # Also when button is pressed, save the current time it was pressed and clear any delayed execution timer that existed
            self.button_pressing_log[button_idx].append(time.time())
            self.button_pressing_log[button_idx] = self.button_pressing_log[button_idx][-2:]  # Keep only last 2 records (needed to check double presses)
            if self.button_timers[button_idx] is not None:
                self.button_timers[button_idx].setClearTimer()
        else:
            # Trigger the raw release action
            if not self.ui_state_manager.block_ui_input:
                self.ui_state_manager.current_state.on_button_up(button_idx, shift=self.is_shift_pressed())

            # Also when button is released check the following:
            # * If pressed for longer than BUTTON_LONG_PRESS_TIME, trigger a "long press" action
            # * If pressed for shorter than BUTTON_LONG_PRESS_TIME, set up a delayed action to trigger the button press and decide whether it should be "single" or "double" press
            last_time_pressed = self.button_pressing_log[button_idx][-1]
            if time.time() - last_time_pressed > BUTTON_LONG_PRESS_TIME:
                # If button pressed for long time, trigger long press action
                if not self.ui_state_manager.block_ui_input:
                    self.ui_state_manager.current_state.on_button_long_pressed(button_idx, shift=self.is_shift_pressed())
            else:
                self.button_timers[button_idx] = Timer()
                self.button_timers[button_idx].setTimeout(delayed_double_press_button_check, [button_idx], BUTTON_DOUBLE_PRESS_TIME)

        # Updated stored button's state
        self.buttons_state[button_idx] = int(val)

    def handle_encoder_button(self, val):

        def delayed_double_press_encoder_check():
            last_time_pressed = self.encoder_pressing_log[-1]
            try:
                previous_time_pressed = self.encoder_pressing_log[-2]
            except IndexError:
                previous_time_pressed = 0
            if last_time_pressed - previous_time_pressed < BUTTON_DOUBLE_PRESS_TIME:
                # If time between last 2 pressings is shorter than BUTTON_DOUBLE_PRESS_TIME, trigger double press action
                self.ui_state_manager.current_state.on_encoder_double_pressed(shift=self.is_shift_pressed())
            else:
                self.ui_state_manager.current_state.on_encoder_pressed(shift=self.is_shift_pressed())

        is_being_pressed = int(val) == 1
        if is_being_pressed:
            # Trigger the raw press action
            self.ui_state_manager.current_state.on_encoder_down(shift=self.is_shift_pressed())

            # Also when encoder is pressed, save the current time it was pressed and clear any delayed execution timer that existed
            self.encoder_pressing_log.append(time.time())
            self.encoder_pressing_log = self.encoder_pressing_log[-2:]  # Keep only last 2 records (needed to check double presses)
            if self.encoder_timer is not None:
                self.encoder_timer.setClearTimer()
        else:
            # Trigger the raw release action
            if not self.ui_state_manager.block_ui_input:
                self.ui_state_manager.current_state.on_encoder_up(shift=self.is_shift_pressed())

            # Also when encoder is released check the following:
            # * If pressed for longer than BUTTON_LONG_PRESS_TIME, trigger a "long press" action
            # * If pressed for shorter than BUTTON_LONG_PRESS_TIME, set up a delayed action to trigger the encoder press and decide whether it should be "single" or "double" press
            last_time_pressed = self.encoder_pressing_log[-1]
            if time.time() - last_time_pressed > BUTTON_LONG_PRESS_TIME:
                # If encoder pressed for long time, trigger long press action
                if not self.ui_state_manager.block_ui_input:
                    self.ui_state_manager.current_state.on_encoder_long_pressed(shift=self.is_shift_pressed())
            else:
                self.encoder_timer = Timer()
                self.encoder_timer.setTimeout(delayed_double_press_encoder_check, [], BUTTON_DOUBLE_PRESS_TIME)

    def handle_encoder(self, direction):
        if not self.ui_state_manager.block_ui_input:
            self.ui_state_manager.current_state.on_encoder_rotated(direction, shift=self.is_shift_pressed())

    def run(self):

        if self.uic is not None:
            self.uic.run()

        # Start loop to call refresh for the display
        starttime = time.time()
        counter = 0
        leds_have_been_unset_after_animation = False
        while True:
            time.sleep(1.0/self.refresh_fps)
            counter += 1
            time_since_start = time.time() - starttime
            if time_since_start < START_ANIMATION_DURATION and self.ui_state_manager.should_show_start_animation:
                frame = frame_from_start_animation(time_since_start/START_ANIMATION_DURATION, counter)
                if self.uic is not None:
                    led_counter = counter % (N_LEDS + 1)
                    if led_counter == 0:
                        # Stop all leds
                        for i in range(0, N_LEDS):
                            self.uic.set_led(i, 0)
                    else:
                        # Light current led
                        self.uic.set_led(led_counter - 1, 1)
            else:
                if self.ui_state_manager.should_show_start_animation is True:
                    self.ui_state_manager.should_show_start_animation = False

                if self.uic is not None:
                    if not leds_have_been_unset_after_animation:
                        for i in range(0, N_LEDS):
                            self.uic.set_led(i, 0)
                        leds_have_been_unset_after_animation = True
                frame = self.compute_display_frame(counter)

            # Save frame to "global_frame" var (used by simulator)
            self.current_frame = frame

            # Send new frame to OLED display
            if self.uic is not None:
                self.uic.set_display_frame(frame)
                self.uic.refresh()

    def compute_display_frame(self, counter):

        # Process plugin state information and send it to state manager
        source_extra_state = {}

        # Check plugin connection status
        source_extra_state[StateNames.CONNECTION_WITH_PLUGIN_OK] = not self.source_plugin_interface.sss.plugin_may_be_down()
        if not source_extra_state[StateNames.CONNECTION_WITH_PLUGIN_OK]:
            self.ui_state_manager.show_global_message("Pl. disconnected :(")

        # Check network connection status
        source_extra_state[StateNames.NETWORK_IS_CONNECTED] = '(R)' not in system_stats.get("network_ssid", "-") and system_stats.get("network_ssid", "-").lower() != 'no network'

        # Add other system stats
        source_extra_state[StateNames.SYSTEM_STATS] = system_stats

        # Send processed state to state manager
        self.source_plugin_interface.update_source_extra_state(source_extra_state)

        # Generate new frame
        frame = self.ui_state_manager.draw_display_frame()

        return frame
