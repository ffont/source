import definitions
import push2_python
import time

class MainControlsMode(definitions.ShepherdControllerMode):

    track_triggering_button_pressing_time = None
    preset_selection_button_pressing_time = None
    button_quick_press_time = 0.400

    last_tap_tempo_times = []

    toggle_display_button = push2_python.constants.BUTTON_USER
    settings_button = push2_python.constants.BUTTON_SETUP
    melodic_rhythmic_toggle_button = push2_python.constants.BUTTON_NOTE
    track_triggering_button = push2_python.constants.BUTTON_SESSION
    preset_selection_mode_button = push2_python.constants.BUTTON_ADD_DEVICE
    ddrm_tone_selection_mode_button = push2_python.constants.BUTTON_DEVICE
    shift_button = push2_python.constants.BUTTON_SHIFT
    select_button = push2_python.constants.BUTTON_SELECT
    play_button = push2_python.constants.BUTTON_PLAY
    record_button = push2_python.constants.BUTTON_RECORD
    metronome_button = push2_python.constants.BUTTON_METRONOME
    tap_tempo_button = push2_python.constants.BUTTON_TAP_TEMPO
    fixed_length_button = push2_python.constants.BUTTON_FIXED_LENGTH
    record_automation_button = push2_python.constants.BUTTON_AUTOMATE

    buttons_used = [toggle_display_button, settings_button, melodic_rhythmic_toggle_button, track_triggering_button, preset_selection_mode_button, 
                    ddrm_tone_selection_mode_button, shift_button, select_button, play_button, record_button, metronome_button, fixed_length_button,
                    record_automation_button]

    def activate(self):
        self.update_buttons()

    def get_transport_buttons_state(self):
        is_playing = self.session.playing
        is_recording = False
        for track in self.session.tracks:
            for clip in track.clips:
                clip_state = clip.get_status()
                if 'r' in clip_state or 'w' in clip_state or 'W' in clip_state:
                    is_recording = True
                    break
        metronome_on = self.session.metronome_on
        return is_playing, is_recording, metronome_on

    def update_buttons(self):
        # Shift and select button
        self.set_button_color_if_pressed(self.shift_button, animation=definitions.DEFAULT_ANIMATION)
        self.set_button_color_if_pressed(self.select_button, animation=definitions.DEFAULT_ANIMATION)

        # Note button, to toggle melodic/rhythmic mode
        self.set_button_color(self.melodic_rhythmic_toggle_button)

        # Button to toggle display on/off
        self.set_button_color_if_expression(self.toggle_display_button, self.app.use_push2_display)
        
        # Settings button, to toggle settings mode
        self.set_button_color_if_expression(self.settings_button, self.app.is_mode_active(self.app.settings_mode), animation=definitions.DEFAULT_ANIMATION)

        # Track triggering mode
        self.set_button_color_if_expression(self.track_triggering_button, self.app.is_mode_active(self.app.clip_triggering_mode), animation=definitions.DEFAULT_ANIMATION)

        # Preset selection mode
        self.set_button_color_if_expression(self.preset_selection_mode_button, self.app.is_mode_active(self.app.preset_selection_mode), animation=definitions.DEFAULT_ANIMATION)

        # DDRM tone selector mode
        if self.app.ddrm_tone_selector_mode.should_be_enabled():
            self.set_button_color_if_expression(self.ddrm_tone_selection_mode_button, self.app.is_mode_active(self.app.ddrm_tone_selector_mode), animation=definitions.DEFAULT_ANIMATION)
        else:
            self.set_button_color(self.ddrm_tone_selection_mode_button, definitions.BLACK)

        # Play/stop/metronome buttons
        is_playing, is_recording, metronome_on = self.get_transport_buttons_state()
        self.set_button_color_if_expression(self.play_button, is_playing, definitions.GREEN, false_color=definitions.WHITE)
        self.set_button_color_if_expression(self.record_button, is_recording, definitions.RED, false_color=definitions.WHITE, animation=definitions.DEFAULT_ANIMATION if self.app.is_button_being_pressed(self.record_button) else definitions.ANIMATION_STATIC, also_include_is_pressed=True)
        self.set_button_color_if_expression(self.metronome_button, metronome_on, definitions.WHITE)
        self.set_button_color(self.tap_tempo_button)

        # Fixed length button
        fixed_length_amount = self.session.fixed_length_recording_bars
        self.set_button_color_if_expression(self.fixed_length_button, fixed_length_amount > 0.0, animation=definitions.DEFAULT_ANIMATION)
        
        # Record automation button
        record_automation_enabled = self.session.record_automation_enabled
        self.set_button_color_if_expression(self.record_automation_button, record_automation_enabled, color=definitions.RED)

    def global_record(self):
        # Stop all clips that are being recorded
        # If the currently played clip in currently selected track is not recording, start recording it
        selected_trak_num = self.app.track_selection_mode.selected_track
        for track_num, track in enumerate(self.session.tracks):
            if track_num == selected_trak_num:
                clip_num = -1
                for i, clip in enumerate(track.clips):
                    clip_state = clip.get_status()
                    if 'p' in clip_state or 'w' in clip_state:
                        # clip is playing or cued to record, toggle recording on that clip
                        clip_num = i
                        break
                if clip_num > -1:
                    self.session.get_clip_by_idx(track_num, clip_num).record_on_off()
            else:
                for clip_num, clip in enumerate(track.clips):
                    clip_state = clip.get_status()
                    if 'r' in clip_state or 'w' in clip_state:
                        # if clip is recording or cued to record, toggle record so recording/cue are cleared
                        clip.record_on_off()

    def on_button_pressed(self, button_name, shift=False, select=False, long_press=False, double_press=False):
        if button_name == self.melodic_rhythmic_toggle_button:
            self.app.toggle_melodic_rhythmic_slice_modes()
            self.app.pads_need_update = True
            return True

        elif button_name == self.settings_button:
            self.app.toggle_and_rotate_settings_mode()
            return True

        elif button_name == self.toggle_display_button:
            self.app.use_push2_display = not self.app.use_push2_display
            if not self.app.use_push2_display:
                self.push.display.send_to_display(self.push.display.prepare_frame(self.push.display.make_black_frame()))
            return True

        elif button_name == self.ddrm_tone_selection_mode_button:
            if self.app.ddrm_tone_selector_mode.should_be_enabled():
                self.app.toggle_ddrm_tone_selector_mode()
            return True

        elif button_name == self.play_button:
            self.session.play_stop()
            return True 
            
        elif button_name == self.record_button:
            if not long_press:
                # Ignore long press event as it can be triggered while making a rec+pad button combination
                self.global_record()
            return True  

        elif button_name == self.metronome_button:
            self.session.metronome_on_off()
            self.app.add_display_notification(
                "Metronome: {0}".format('On' if not self.session.metronome_on else 'Off'))
            return True  

        elif button_name == self.tap_tempo_button:
            self.last_tap_tempo_times.append(time.time())
            if len(self.last_tap_tempo_times) >= 3:
                intervals = []
                for t1, t2 in zip(reversed(self.last_tap_tempo_times[-2:]), reversed(self.last_tap_tempo_times[-3:-1])):
                    intervals.append(t1 - t2)
                bpm = 60.0 / (sum(intervals)/len(intervals))
                if 30 <= bpm <= 300:
                    self.session.set_bpm(float(int(bpm)))
                    self.last_tap_tempo_times = self.last_tap_tempo_times[-3:]
                    self.app.add_display_notification("Tempo: {0} bpm".format(int(bpm)))
            return True

        elif button_name == self.fixed_length_button:
            if long_press:
                next_fixed_length = 0
            else:
                current_fixed_length = self.session.fixed_length_recording_bars
                next_fixed_length = current_fixed_length + 1
                if next_fixed_length > 8:
                    next_fixed_length = 0
            self.session.set_fix_length_recording_bars(next_fixed_length)

            if next_fixed_length > 0:
                self.app.add_display_notification(
                    "Fixed length bars: {0} ({1} beats)"
                        .format(next_fixed_length, next_fixed_length * self.session.meter))
            else:
                self.app.add_display_notification("No fixed length recording")

        elif button_name == self.record_automation_button:
            self.session.set_record_automation_enabled()

        elif button_name == push2_python.constants.BUTTON_MASTER:
            # Toggle backend sine-wave debug synth
            self.state.toggle_shepherd_backend_debug_synth()

    def on_button_pressed_raw(self, button_name):    
        if button_name == self.track_triggering_button:
            if self.app.is_mode_active(self.app.clip_triggering_mode):
                # If already active, deactivate and set pressing time to None
                self.app.unset_clip_triggering_mode()
                self.track_triggering_button_pressing_time = None
            else:
                # Activate track triggering mode and store time button pressed
                self.app.set_clip_triggering_mode()
                self.track_triggering_button_pressing_time = time.time()
            return True

        elif button_name == self.preset_selection_mode_button:
            if self.app.is_mode_active(self.app.preset_selection_mode):
                # If already active, deactivate and set pressing time to None
                self.app.unset_preset_selection_mode()
                self.preset_selection_button_pressing_time = None
            else:
                # Activate preset selection mode and store time button pressed
                self.app.set_preset_selection_mode()
                self.preset_selection_button_pressing_time = time.time()
            return True
    
    def on_button_released_raw(self, button_name):
        if button_name == self.track_triggering_button:
            # Decide if short press or long press
            pressing_time = self.track_triggering_button_pressing_time
            is_long_press = False
            if pressing_time is None:
                # Consider quick press (this should not happen pressing time should have been set before)
                pass
            else:
                if time.time() - pressing_time > self.button_quick_press_time:
                    # Consider this is a long press
                    is_long_press = True
                self.track_triggering_button_pressing_time = None
            if is_long_press:
                # If long press, deactivate track triggering mode, else do nothing
                self.app.unset_clip_triggering_mode()
            return True

        elif button_name == self.preset_selection_mode_button:
            # Decide if short press or long press
            pressing_time = self.preset_selection_button_pressing_time
            is_long_press = False
            if pressing_time is None:
                # Consider quick press (this should not happen pressing time should have been set before)
                pass
            else:
                if time.time() - pressing_time > self.button_quick_press_time:
                    # Consider this is a long press
                    is_long_press = True
                self.preset_selection_button_pressing_time = None
            if is_long_press:
                # If long press, deactivate preset selection mode, else do nothing
                self.app.unset_preset_selection_mode()
            return True

    def on_encoder_rotated(self, encoder_name, increment):
        if encoder_name == push2_python.constants.ENCODER_TEMPO_ENCODER:
            if not self.app.is_button_being_pressed(push2_python.constants.BUTTON_SHIFT):
                new_bpm = self.session.bpm + increment
                if new_bpm < 10.0:
                    new_bpm = 10.0
                self.session.set_bpm(new_bpm)
                self.app.add_display_notification("Tempo: {0} bpm".format(int(new_bpm)))
                return True
            else:
                new_meter = self.session.meter + increment
                if new_meter < 1:
                    new_meter = 1
                self.session.set_meter(new_meter)
                self.app.add_display_notification("Meter: {0} beats".format(new_meter))
                return True
