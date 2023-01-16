import json
import math
import os
import subprocess
import time
import traceback
import sys

import cairo
import definitions
import mido
import numpy
import push2_python

from collections import defaultdict

from modes.melodic_mode import MelodicMode
from modes.track_selection_mode import TrackSelectionMode
from modes.clip_triggering_mode import ClipTriggeringMode
from modes.clip_edit_mode import ClipEditgMode
from modes.rhythmic_mode import RhythmicMode
from modes.slice_notes_mode import SliceNotesMode
from modes.settings_mode import SettingsMode
from modes.main_controls_mode import MainControlsMode
from modes.midi_cc_mode import MIDICCMode
from modes.preset_selection_mode import PresetSelectionMode
from modes.ddrm_tone_selector_mode import DDRMToneSelectorMode
from utils import show_notification

# Add specific directory to python so we can import pyshepherd
sys.path.append(os.path.join(os.path.dirname(os.path.dirname(os.path.realpath(__file__))), 'SourceSampler/3rdParty/shepherd/'))
from pyshepherd.pyshepherd import ShepherdBackendControllerApp


class SourceShepherdPush2ControllerApp(ShepherdBackendControllerApp):
    # midi
    available_midi_in_device_names = []
    notes_midi_in = None  # MIDI input device only used to receive note messages and illuminate pads/keys
    last_attempt_configuring_notes_in = 0

    # push
    push = None
    use_push2_display = None
    target_frame_rate = None

    # frame rate measurements
    actual_frame_rate = 0
    current_frame_rate_measurement = 0
    current_frame_rate_measurement_second = 0

    # other state vars
    modes_initialized = False
    active_modes = []
    previously_active_mode_for_xor_group = {}
    active_modes_need_reactivate = False
    pads_need_update = True
    buttons_need_update = True
    showing_countin_message = False

    # notifications
    notification_text = None
    notification_time = 0

    # fixing issue with 2 lumis and alternating channel pressure values
    last_cp_value_recevied = 0
    last_cp_value_recevied_time = 0

    # interface with shepherd
    shepherd_interface = None

    def __init__(self, *args, **kwargs):

        # Start push
        settings = self.load_settings_from_file()
        self.target_frame_rate = settings.get('target_frame_rate', 60)
        self.use_push2_display = settings.get('use_push2_display', True)
        self.init_push()

        # Start backend (if indicated in settings file)
        run_backend_from_frontend = settings.get("run_backend_from_frontend", False)
        if run_backend_from_frontend:
            # If "run_backend_from_frontend" is set in settings.json as a command list to be
            # run by subprocess, do it (e.g. ["xvfb-run", "-a", "../Shepherd/Builds/LinuxMakefile/build/Shepherd"])
            subprocess.Popen(run_backend_from_frontend)

        time.sleep(0.5)  # Give some time for push to initialize and backend (if need be)

        # Start shepherd interface (do that by calling the super class method)
        super().__init__(*args, **kwargs)

        # NOTE: app modes will be initialized once first state has been received

    def load_settings_from_file(self):
        if os.path.exists(definitions.SETTINGS_FILE_PATH):
            settings = json.load(open(definitions.SETTINGS_FILE_PATH))
        else:
            settings = {}
        return settings

    def on_full_state_received(self):
        if not self.modes_initialized:
            self.init_modes(self.load_settings_from_file())
        else:
            self.active_modes_need_reactivate = True

    def on_backend_connection_lost(self):
        # Set notes midi in to None as virtual midi device created by backend might have disappeared.
        # In this way we will try re-connecting until it is ready again
        self.notes_midi_in = None

    def on_state_update_received(self, update_data):
        if self.shepherd_interface.state is None or not self.modes_initialized: return

        # Check if playhead is changing while doing count in, and show notification message
        if update_data['updateType'] == 'propertyChanged':
            property_name = update_data['propertyName']
            if update_data['affectedElement'] == self.session \
                    and (property_name == 'playhead_position_in_beats' or
                         property_name == 'count_in_playhead_position_in_beats'):
                if self.session.doing_count_in:
                    self.showing_countin_message = True
                    self.add_display_notification("Will start recording in: {0:.0f}"
                                                  .format(math.ceil(4 - self.session.count_in_playhead_position_in_beats)))
                else:
                    if self.showing_countin_message:
                        self.clear_display_notification()
                        self.showing_countin_message = False

        # Trigger re-activation of modes in case pads need to be updated
        # TODO: this should be optimized, we should interpret update_data in an intelligent way and decide
        # TODO: what to update from every state to avoid doing more work than necessary
        self.active_modes_need_reactivate = True

    def init_modes(self, settings):
        print('Initializing app modes...')
        self.main_controls_mode = MainControlsMode(self, settings=settings)
        self.active_modes.append(self.main_controls_mode)

        self.melodic_mode = MelodicMode(self, settings=settings)
        self.rhyhtmic_mode = RhythmicMode(self, settings=settings)
        self.slice_notes_mode = SliceNotesMode(self, settings=settings)
        self.set_melodic_mode()

        self.track_selection_mode = TrackSelectionMode(self, settings=settings)
        self.clip_triggering_mode = ClipTriggeringMode(self, settings=settings)
        self.clip_edit_mode = ClipEditgMode(self, settings=settings)
        self.preset_selection_mode = PresetSelectionMode(self, settings=settings)
        self.midi_cc_mode = MIDICCMode(self,
                                       settings=settings)  # Must be initialized after track selection mode so it gets info about loaded tracks
        self.active_modes += [self.track_selection_mode, self.midi_cc_mode]
        self.track_selection_mode.select_track(self.track_selection_mode.selected_track)
        self.ddrm_tone_selector_mode = DDRMToneSelectorMode(self, settings=settings)

        self.settings_mode = SettingsMode(self, settings=settings)

        self.modes_initialized = True
        print('App modes initialized!')

    def get_all_modes(self):
        return [getattr(self, element) for element in vars(self) if
                isinstance(getattr(self, element), definitions.ShepherdControllerMode)]

    def is_mode_active(self, mode):
        return mode in self.active_modes

    def toggle_and_rotate_settings_mode(self):
        if self.is_mode_active(self.settings_mode):
            rotation_finished = self.settings_mode.move_to_next_page()
            if rotation_finished:
                self.active_modes = [mode for mode in self.active_modes if mode != self.settings_mode]
                self.settings_mode.deactivate()
        else:
            self.active_modes.append(self.settings_mode)
            self.settings_mode.activate()

    def toggle_ddrm_tone_selector_mode(self):
        if self.is_mode_active(self.ddrm_tone_selector_mode):
            # Deactivate (replace ddrm tone selector mode by midi cc and track selection mode)
            new_active_modes = []
            for mode in self.active_modes:
                if mode != self.ddrm_tone_selector_mode:
                    new_active_modes.append(mode)
                else:
                    new_active_modes.append(self.track_selection_mode)
                    new_active_modes.append(self.midi_cc_mode)
            self.active_modes = new_active_modes
            self.ddrm_tone_selector_mode.deactivate()
            self.midi_cc_mode.activate()
            self.track_selection_mode.activate()
        else:
            # Activate (replace midi cc and track selection mode by ddrm tone selector mode)
            new_active_modes = []
            for mode in self.active_modes:
                if mode != self.track_selection_mode and mode != self.midi_cc_mode:
                    new_active_modes.append(mode)
                elif mode == self.midi_cc_mode:
                    new_active_modes.append(self.ddrm_tone_selector_mode)
            self.active_modes = new_active_modes
            self.midi_cc_mode.deactivate()
            self.track_selection_mode.deactivate()
            self.ddrm_tone_selector_mode.activate()

    def set_mode_for_xor_group(self, mode_to_set):
        '''This activates the mode_to_set, but makes sure that if any other modes are currently activated
        for the same xor_group, these other modes get deactivated. This also stores a reference to the
        latest active mode for xor_group, so once a mode gets unset, the previously active one can be
        automatically set'''

        if not self.is_mode_active(mode_to_set):

            # First deactivate all existing modes for that xor group
            new_active_modes = []
            for mode in self.active_modes:
                if mode.xor_group is not None and mode.xor_group == mode_to_set.xor_group:
                    mode.deactivate()
                    self.previously_active_mode_for_xor_group[
                        mode.xor_group] = mode  # Store last mode that was active for the group
                else:
                    new_active_modes.append(mode)
            self.active_modes = new_active_modes

            # Now add the mode to set to the active modes list and activate it
            new_active_modes.append(mode_to_set)
            mode_to_set.activate()

    def unset_mode_for_xor_group(self, mode_to_unset):
        '''This deactivates the mode_to_unset and reactivates the previous mode that was active for this xor_group.
        This allows to make sure that one (and onyl one) mode will be always active for a given xor_group.
        '''
        if self.is_mode_active(mode_to_unset):

            # Deactivate the mode to unset
            self.active_modes = [mode for mode in self.active_modes if mode != mode_to_unset]
            mode_to_unset.deactivate()

            # Activate the previous mode that was activated for the same xor_group. If none listed, activate a default one
            previous_mode = self.previously_active_mode_for_xor_group.get(mode_to_unset.xor_group, None)
            if previous_mode is not None:
                del self.previously_active_mode_for_xor_group[mode_to_unset.xor_group]
                self.set_mode_for_xor_group(previous_mode)
            else:
                # Enable default
                # TODO: here we hardcoded the default mode for a specific xor_group, I should clean this a little bit in the future...
                if mode_to_unset.xor_group == 'pads':
                    self.set_mode_for_xor_group(self.melodic_mode)

    def toggle_melodic_rhythmic_slice_modes(self):
        if self.is_mode_active(self.melodic_mode):
            self.set_rhythmic_mode()
        elif self.is_mode_active(self.rhyhtmic_mode):
            self.set_slice_notes_mode()
        elif self.is_mode_active(self.slice_notes_mode):
            self.set_melodic_mode()
        else:
            # If none of melodic or rhythmic or slice modes were active, enable melodic by default
            self.set_melodic_mode()

    def set_melodic_mode(self):
        self.set_mode_for_xor_group(self.melodic_mode)

    def set_rhythmic_mode(self):
        self.set_mode_for_xor_group(self.rhyhtmic_mode)

    def set_slice_notes_mode(self):
        self.set_mode_for_xor_group(self.slice_notes_mode)

    def set_clip_triggering_mode(self):
        self.set_mode_for_xor_group(self.clip_triggering_mode)

    def unset_clip_triggering_mode(self):
        self.unset_mode_for_xor_group(self.clip_triggering_mode)

    def set_clip_edit_mode(self):
        self.set_mode_for_xor_group(self.clip_edit_mode)

    def unset_clip_edit_mode(self):
        self.unset_mode_for_xor_group(self.clip_edit_mode)

    def set_preset_selection_mode(self):
        self.set_mode_for_xor_group(self.preset_selection_mode)

    def unset_preset_selection_mode(self):
        self.unset_mode_for_xor_group(self.preset_selection_mode)

    def save_current_settings_to_file(self):
        settings = {
            'use_push2_display': self.use_push2_display,
            'target_frame_rate': self.target_frame_rate,
        }
        for mode in self.get_all_modes():
            mode_settings = mode.get_settings_to_save()
            if mode_settings:
                settings.update(mode_settings)
        json.dump(settings, open(definitions.SETTINGS_FILE_PATH, 'w'))

    def init_notes_midi_in(self, device_name=None):
        print('Configuring notes MIDI in to {}...'.format(device_name))
        self.available_midi_in_device_names = [name for name in mido.get_input_names() if
                                               'Ableton Push' not in name and 'RtMidi' not in name and 'Through' not in name]

        if device_name is not None:
            try:
                full_name = [name for name in self.available_midi_in_device_names if device_name in name][0]
            except IndexError:
                full_name = None
            if full_name is not None:
                if self.notes_midi_in is not None:
                    self.notes_midi_in.callback = None  # Disable current callback first (if any)
                try:
                    self.notes_midi_in = mido.open_input(full_name)
                    self.notes_midi_in.callback = self.notes_midi_in_handler
                    print('Receiving notes MIDI in from "{0}"'.format(full_name))
                except IOError:
                    print('Could not connect to notes MIDI input port "{}"'.format(full_name))
                    print('- Available device names: {}'.format(
                        ','.join([name for name in self.available_midi_in_device_names])))
                except Exception as e:
                    print('Could not connect to notes MIDI input port "{}"'.format(full_name))
                    print('- Unexpected error occurred: {}'.format(str(e)))
            else:
                print('No available device name found for {}'.format(device_name))
        else:
            if self.notes_midi_in is not None:
                self.notes_midi_in.callback = None  # Disable current callback (if any)
                self.notes_midi_in.close()
                self.notes_midi_in = None

        if self.notes_midi_in is None:
            print('Could not configure notes MIDI input')

    def notes_midi_in_handler(self, msg):
        # Check if message is note on or off and if that is the case, send message to the melodic/rhythmic active modes 
        # so the notes are shown in pads/keys
        if msg.type == 'note_on' or msg.type == 'note_off':
            for mode in self.active_modes:
                if mode == self.melodic_mode or mode == self.rhyhtmic_mode or mode == self.slice_notes_mode:
                    mode.on_midi_in(msg, source=self.notes_midi_in.name)
                    if mode.lumi_midi_out is not None:
                        mode.lumi_midi_out.send(msg)
                    else:
                        # If midi not properly initialized try to re-initialize but don't do it too often
                        if time.time() - mode.last_time_tried_initialize_lumi > 5:
                            mode.init_lumi_midi_out()

    def add_display_notification(self, text):
        self.notification_text = text
        self.notification_time = time.time()

    def clear_display_notification(self):
        self.notification_text = None
        self.notification_time = time.time()

    def init_push(self):
        print('Configuring Push...')
        use_simulator = not definitions.RUNNING_ON_RPI
        simulator_port = 6128
        if use_simulator:
            print('Using Push2 simulator at http://localhost:{}'.format(simulator_port))
        self.push = push2_python.Push2(run_simulator=use_simulator, simulator_port=simulator_port,
                                       simulator_use_virtual_midi_out=use_simulator)
        if definitions.RUNNING_ON_RPI:
            # When this app runs in Linux is because it is running on the Raspberrypi
            # I've overved problems trying to reconnect many times without success on the Raspberrypi, resulting in
            # "ALSA lib seq_hw.c:466:(snd_seq_hw_open) open /dev/snd/seq failed: Cannot allocate memory" issues.
            # A work around is make the reconnection time bigger, but a better solution should probably be found.
            self.push.set_push2_reconnect_call_interval(2)

    def update_push2_pads(self):
        if self.shepherd_interface.state is None or not self.modes_initialized: return
        for mode in self.active_modes:
            mode.update_pads()

    def update_push2_buttons(self):
        if self.shepherd_interface.state is None or not self.modes_initialized: return
        for mode in self.active_modes:
            mode.update_buttons()

    def update_push2_display(self):
        if self.shepherd_interface.state is None or not self.modes_initialized: return
        if self.use_push2_display:
            # Prepare cairo canvas
            w, h = push2_python.constants.DISPLAY_LINE_PIXELS, push2_python.constants.DISPLAY_N_LINES
            surface = cairo.ImageSurface(cairo.FORMAT_RGB16_565, w, h)
            ctx = cairo.Context(surface)

            # Call all active modes to write to context
            for mode in self.active_modes:
                mode.update_display(ctx, w, h)

            # Show any notifications that should be shown
            if self.notification_text is not None:
                time_since_notification_started = time.time() - self.notification_time
                if time_since_notification_started < definitions.NOTIFICATION_TIME:
                    show_notification(ctx, self.notification_text,
                                      opacity=1 - time_since_notification_started / definitions.NOTIFICATION_TIME)
                else:
                    self.notification_text = None

            # Convert cairo data to numpy array and send to push
            buf = surface.get_data()
            frame = numpy.ndarray(shape=(h, w), dtype=numpy.uint16, buffer=buf).transpose()
            self.push.display.display_frame(frame, input_format=push2_python.constants.FRAME_FORMAT_RGB565)

    def check_for_delayed_actions(self):
        if self.shepherd_interface.state is None or not self.modes_initialized: return
        # If MIDI not configured, make sure we try sending messages so it gets configured
        if not self.push.midi_is_configured():
            try:
                self.push.configure_midi()
            except:
                # This is to avoid a bug in mido (?) which is triggered when JUCE is
                # creating a virtual output device
                # See https://github.com/pallets/flask/issues/3626
                pass

        # If notes in is not configured, try to do it (but not too often)
        if self.notes_midi_in is None and time.time() - self.last_attempt_configuring_notes_in > 2:
            self.last_attempt_configuring_notes_in = time.time()
            if self.shepherd_interface.state is not None:
                try:
                    self.init_notes_midi_in(device_name=self.shepherd_interface.state.notes_monitoring_device_name)
                except Exception as e:
                    print('Can\'t get information about which notes midi in device to configure: {}'.format(str(e)))

        # If active modes need to be reactivated, do it
        if self.active_modes_need_reactivate:
            for mode in self.active_modes:
                mode.activate()
            self.active_modes_need_reactivate = False

        # Call dalyed actions in active modes
        for mode in self.active_modes:
            mode.check_for_delayed_actions()

        if self.pads_need_update:
            self.update_push2_pads()
            self.pads_need_update = False

        if self.buttons_need_update:
            self.update_push2_buttons()
            self.buttons_need_update = False

    def run_loop(self):
        print('ShepherdController app loop is starting...')
        try:
            while True:
                before_draw_time = time.time()

                # Draw ui
                self.update_push2_display()

                # Frame rate measurement
                now = time.time()
                self.current_frame_rate_measurement += 1
                if time.time() - self.current_frame_rate_measurement_second > 1.0:
                    self.actual_frame_rate = self.current_frame_rate_measurement
                    self.current_frame_rate_measurement = 0
                    self.current_frame_rate_measurement_second = now

                # Check if any delayed actions need to be applied
                self.check_for_delayed_actions()

                after_draw_time = time.time()

                # Calculate sleep time to aproximate the target frame rate
                sleep_time = (1.0 / self.target_frame_rate) - (after_draw_time - before_draw_time)
                if sleep_time > 0:
                    time.sleep(sleep_time)

        except KeyboardInterrupt:
            print('Exiting ShepherdController...')
            self.push.f_stop.set()

    def on_midi_push_connection_established(self):
        # Do initial configuration of Push
        print('Doing initial Push config...')

        # Configure custom color palette
        app.push.color_palette = {}
        for count, color_name in enumerate(definitions.COLORS_NAMES):
            app.push.set_color_palette_entry(count, [color_name, color_name],
                                             rgb=definitions.get_color_rgb_float(color_name), allow_overwrite=True)
        app.push.reapply_color_palette()

        # Initialize all buttons to black, initialize all pads to off
        app.push.buttons.set_all_buttons_color(color=definitions.BLACK)
        app.push.pads.set_all_pads_to_color(color=definitions.BLACK)

        if app.shepherd_interface.state is not None:
            # Iterate over modes and (re-)activate them
            for mode in self.active_modes:
                mode.activate()

            # Update buttons and pads (just in case something was missing!)
            app.update_push2_buttons()
            app.update_push2_pads()

    def is_button_being_pressed(self, button_name):
        global buttons_pressed_state
        return buttons_pressed_state.get(button_name, False)

    def set_button_ignore_next_action_if_not_yet_triggered(self, button_name):
        global buttons_should_ignore_next_release_action, buttons_waiting_to_trigger_processed_action
        if buttons_waiting_to_trigger_processed_action.get(button_name, False):
            buttons_should_ignore_next_release_action[button_name] = True


# Bind push action handlers with class methods
@push2_python.on_encoder_rotated()
def on_encoder_rotated(_, encoder_name, increment):
    if app.shepherd_interface.state is None: return
    try:
        for mode in app.active_modes[::-1]:
            action_performed = mode.on_encoder_rotated(encoder_name, increment)
            if action_performed:
                break  # If mode took action, stop event propagation
    except NameError as e:
        print('Error:  {}'.format(str(e)))
        traceback.print_exc()


pads_pressing_log = defaultdict(list)
pads_timers = defaultdict(None)
pads_pressed_state = {}
pads_should_ignore_next_release_action = {}
pads_last_pressed_veocity = {}


@push2_python.on_pad_pressed()
def on_pad_pressed(_, pad_n, pad_ij, velocity):
    if app.shepherd_interface.state is None: return
    global pads_pressing_log, pads_timers, pads_pressed_state, pads_should_ignore_next_release_action

    # - Trigger raw pad pressed action
    try:
        for mode in app.active_modes[::-1]:
            action_performed = mode.on_pad_pressed_raw(pad_n, pad_ij, velocity)
            if action_performed:
                break  # If mode took action, stop event propagation
    except NameError as e:
        print('Error:  {}'.format(str(e)))
        traceback.print_exc()

    # - Trigger processed pad actions
    def delayed_long_press_pad_check(pad_n, pad_ij, velocity):
        # If the maximum time to consider a long press has passed and pad has not yet been released,
        # trigger the long press pad action already and make sure when pad is actually released
        # no new processed pad action is triggered
        if pads_pressed_state.get(pad_n, False):
            # If pad has not been released, trigger the long press action
            try:
                for mode in app.active_modes[::-1]:
                    action_performed = mode.on_pad_pressed(pad_n, pad_ij, velocity, long_press=True,
                                                           shift=buttons_pressed_state.get(
                                                               push2_python.constants.BUTTON_SHIFT, False),
                                                           select=buttons_pressed_state.get(
                                                               push2_python.constants.BUTTON_SELECT, False))
                    if action_performed:
                        break  # If mode took action, stop event propagation
            except NameError as e:
                print('Error:  {}'.format(str(e)))
                traceback.print_exc()

            # Store that next release action should be ignored so that long press action is not retriggered when actual pad release takes place
            pads_should_ignore_next_release_action[pad_n] = True

    # Save the current time the pad is pressed and clear any delayed execution timer that existed
    # Also save velocity of the current pressing as it will be used when triggering the actual porcessed action when release action is triggered
    pads_last_pressed_veocity[pad_n] = velocity
    pads_pressing_log[pad_n].append(time.time())
    pads_pressing_log[pad_n] = pads_pressing_log[pad_n][
                               -2:]  # Keep only last 2 records (needed to check double presses)
    if pads_timers.get(pad_n, None) is not None:
        pads_timers[pad_n].setClearTimer()

    # Schedule a delayed action for the pad long press that will fire as soon as the pad is being pressed for more than definitions.BUTTON_LONG_PRESS_TIME
    pads_timers[pad_n] = definitions.Timer()
    pads_timers[pad_n].setTimeout(delayed_long_press_pad_check, [pad_n, pad_ij, velocity],
                                  definitions.BUTTON_LONG_PRESS_TIME)

    # - Store pad pressed state
    pads_pressed_state[pad_n] = True


@push2_python.on_pad_released()
def on_pad_released(_, pad_n, pad_ij, velocity):
    if app.shepherd_interface.state is None: return
    global pads_pressing_log, pads_timers, pads_pressed_state, pads_should_ignore_next_release_action

    # - Trigger raw pad released action
    try:
        for mode in app.active_modes[::-1]:
            action_performed = mode.on_pad_released_raw(pad_n, pad_ij, velocity)
            if action_performed:
                break  # If mode took action, stop event propagation
    except NameError as e:
        print('Error:  {}'.format(str(e)))
        traceback.print_exc()

    # - Trigger processed pad actions
    def delayed_double_press_pad_check(pad_n, pad_ij, velocity):
        last_time_pressed = pads_pressing_log[pad_n][-1]
        try:
            previous_time_pressed = pads_pressing_log[pad_n][-2]
        except IndexError:
            previous_time_pressed = 0
        if last_time_pressed - previous_time_pressed < definitions.BUTTON_DOUBLE_PRESS_TIME:
            # If time between last 2 pressings is shorter than BUTTON_DOUBLE_PRESS_TIME, trigger double press action
            try:
                for mode in app.active_modes[::-1]:
                    action_performed = mode.on_pad_pressed(pad_n, pad_ij, velocity, double_press=True,
                                                           shift=buttons_pressed_state.get(
                                                               push2_python.constants.BUTTON_SHIFT, False),
                                                           select=buttons_pressed_state.get(
                                                               push2_python.constants.BUTTON_SELECT, False))
                    if action_performed:
                        break  # If mode took action, stop event propagation
            except NameError as e:
                print('Error:  {}'.format(str(e)))
                traceback.print_exc()
        else:
            try:
                for mode in app.active_modes[::-1]:
                    action_performed = mode.on_pad_pressed(pad_n, pad_ij, velocity,
                                                           shift=buttons_pressed_state.get(
                                                               push2_python.constants.BUTTON_SHIFT, False),
                                                           select=buttons_pressed_state.get(
                                                               push2_python.constants.BUTTON_SELECT, False))
                    if action_performed:
                        break  # If mode took action, stop event propagation
            except NameError as e:
                print('Error:  {}'.format(str(e)))
                traceback.print_exc()

    if not pads_should_ignore_next_release_action.get(pad_n, False):
        # If pad is not marked to ignore the next release action, then use the delayed_double_press_pad_check to decide whether
        # a "normal press" or a "double press" should be triggered
        # Clear any delayed execution timer that existed to avoid duplicated events
        if pads_timers.get(pad_n, None) is not None:
            pads_timers[pad_n].setClearTimer()
        pads_timers[pad_n] = definitions.Timer()
        velocity_of_press_action = pads_last_pressed_veocity.get(pad_n, velocity)
        pads_timers[pad_n].setTimeout(delayed_double_press_pad_check, [pad_n, pad_ij, velocity_of_press_action],
                                      definitions.BUTTON_DOUBLE_PRESS_TIME)
    else:
        pads_should_ignore_next_release_action[pad_n] = False

    # Store pad pressed state
    pads_pressed_state[pad_n] = False


@push2_python.on_pad_aftertouch()
def on_pad_aftertouch(_, pad_n, pad_ij, velocity):
    if app.shepherd_interface.state is None: return
    try:
        for mode in app.active_modes[::-1]:
            action_performed = mode.on_pad_aftertouch(pad_n, pad_ij, velocity)
            if action_performed:
                break  # If mode took action, stop event propagation
    except NameError as e:
        print('Error:  {}'.format(str(e)))
        traceback.print_exc()


buttons_pressing_log = defaultdict(list)
buttons_timers = defaultdict(None)
buttons_pressed_state = {}
buttons_should_ignore_next_release_action = {}
buttons_waiting_to_trigger_processed_action = {}


@push2_python.on_button_pressed()
def on_button_pressed(_, name):
    if app.shepherd_interface.state is None: return
    global buttons_pressing_log, buttons_timers, buttons_pressed_state, buttons_should_ignore_next_release_action, buttons_waiting_to_trigger_processed_action

    # - Trigger raw button pressed action
    try:
        for mode in app.active_modes[::-1]:
            action_performed = mode.on_button_pressed_raw(name)
            mode.set_buttons_need_update_if_button_used(name)
            if action_performed:
                break  # If mode took action, stop event propagation
    except NameError as e:
        print('Error:  {}'.format(str(e)))
        traceback.print_exc()

    # - Trigger processed button actions
    buttons_waiting_to_trigger_processed_action[name] = True

    def delayed_long_press_button_check(name):
        # If the maximum time to consider a long press has passed and button has not yet been released,
        # trigger the long press button action already and make sure when button is actually released
        # no new processed button action is triggered
        if buttons_pressed_state.get(name, False):
            # If button has not been released, trigger the long press action
            try:
                for mode in app.active_modes[::-1]:
                    action_performed = mode.on_button_pressed(name, long_press=True,
                                                              shift=buttons_pressed_state.get(
                                                                  push2_python.constants.BUTTON_SHIFT, False),
                                                              select=buttons_pressed_state.get(
                                                                  push2_python.constants.BUTTON_SELECT, False))
                    mode.set_buttons_need_update_if_button_used(name)
                    if action_performed:
                        break  # If mode took action, stop event propagation
                buttons_waiting_to_trigger_processed_action[name] = False
            except NameError as e:
                print('Error:  {}'.format(str(e)))
                traceback.print_exc()

            # Store that next release action should be ignored so that long press action is not retriggered when actual button release takes place
            buttons_should_ignore_next_release_action[name] = True

    # Save the current time the button is pressed and clear any delayed execution timer that existed
    buttons_pressing_log[name].append(time.time())
    buttons_pressing_log[name] = buttons_pressing_log[name][
                                 -2:]  # Keep only last 2 records (needed to check double presses)
    if buttons_timers.get(name, None) is not None:
        buttons_timers[name].setClearTimer()

    # Schedule a delayed action for the button long press that will fire as soon as the button is being pressed for more than definitions.BUTTON_LONG_PRESS_TIME
    buttons_timers[name] = definitions.Timer()
    buttons_timers[name].setTimeout(delayed_long_press_button_check, [name], definitions.BUTTON_LONG_PRESS_TIME)

    # - Store button pressed state
    buttons_pressed_state[name] = True


@push2_python.on_button_released()
def on_button_released(_, name):
    if app.shepherd_interface.state is None: return
    global buttons_pressing_log, buttons_timers, buttons_pressed_state, buttons_should_ignore_next_release_action, buttons_waiting_to_trigger_processed_action

    # - Trigger raw button released action
    try:
        for mode in app.active_modes[::-1]:
            action_performed = mode.on_button_released_raw(name)
            mode.set_buttons_need_update_if_button_used(name)
            if action_performed:
                break  # If mode took action, stop event propagation
    except NameError as e:
        print('Error:  {}'.format(str(e)))
        traceback.print_exc()

    # - Trigger processed button actions
    def delayed_double_press_button_check(name):
        last_time_pressed = buttons_pressing_log[name][-1]
        try:
            previous_time_pressed = buttons_pressing_log[name][-2]
        except IndexError:
            previous_time_pressed = 0
        if last_time_pressed - previous_time_pressed < definitions.BUTTON_DOUBLE_PRESS_TIME:
            # If time between last 2 pressings is shorter than BUTTON_DOUBLE_PRESS_TIME, trigger double press action
            try:
                for mode in app.active_modes[::-1]:
                    action_performed = mode.on_button_pressed(name, double_press=True,
                                                              shift=buttons_pressed_state.get(
                                                                  push2_python.constants.BUTTON_SHIFT, False),
                                                              select=buttons_pressed_state.get(
                                                                  push2_python.constants.BUTTON_SELECT, False))
                    mode.set_buttons_need_update_if_button_used(name)
                    if action_performed:
                        break  # If mode took action, stop event propagation
                buttons_waiting_to_trigger_processed_action[name] = False
            except NameError as e:
                print('Error:  {}'.format(str(e)))
                traceback.print_exc()
        else:
            try:
                for mode in app.active_modes[::-1]:
                    action_performed = mode.on_button_pressed(name,
                                                              shift=buttons_pressed_state.get(
                                                                  push2_python.constants.BUTTON_SHIFT, False),
                                                              select=buttons_pressed_state.get(
                                                                  push2_python.constants.BUTTON_SELECT, False))
                    mode.set_buttons_need_update_if_button_used(name)
                    if action_performed:
                        break  # If mode took action, stop event propagation
                buttons_waiting_to_trigger_processed_action[name] = False
            except NameError as e:
                print('Error:  {}'.format(str(e)))
                traceback.print_exc()

    if not buttons_should_ignore_next_release_action.get(name, False):
        # If button is not marked to ignore the next release action, then use the delayed_double_press_button_check to decide whether
        # a "normal press" or a "double press" should be triggered
        # Clear any delayed execution timer that existed to avoid duplicated events
        if buttons_timers.get(name, None) is not None:
            buttons_timers[name].setClearTimer()
        buttons_timers[name] = definitions.Timer()
        buttons_timers[name].setTimeout(delayed_double_press_button_check, [name], definitions.BUTTON_DOUBLE_PRESS_TIME)
    else:
        buttons_should_ignore_next_release_action[name] = False

    # Store button pressed state
    buttons_pressed_state[name] = False


@push2_python.on_touchstrip()
def on_touchstrip(_, value):
    if app.shepherd_interface.state is None: return
    try:
        for mode in app.active_modes[::-1]:
            action_performed = mode.on_touchstrip(value)
            if action_performed:
                break  # If mode took action, stop event propagation
    except NameError as e:
        print('Error:  {}'.format(str(e)))
        traceback.print_exc()


@push2_python.on_sustain_pedal()
def on_sustain_pedal(_, sustain_on):
    if app.shepherd_interface.state is None: return
    try:
        for mode in app.active_modes[::-1]:
            action_performed = mode.on_sustain_pedal(sustain_on)
            if action_performed:
                break  # If mode took action, stop event propagation
    except NameError as e:
        print('Error:  {}'.format(str(e)))
        traceback.print_exc()


midi_connected_received_before_app = False


@push2_python.on_midi_connected()
def on_midi_connected(_):
    try:
        app.on_midi_push_connection_established()
    except NameError as e:
        global midi_connected_received_before_app
        midi_connected_received_before_app = True
        print('Error:  {}'.format(str(e)))
        traceback.print_exc()


# Run app main loop
if __name__ == "__main__":
    app = SourceShepherdPush2ControllerApp(debugger_port=5100)
    if midi_connected_received_before_app:
        # App received the "on_midi_connected" call before it was initialized. Do it now!
        print('Missed MIDI initialization call, doing it now...')
        app.on_midi_push_connection_established()
    app.run_loop()
