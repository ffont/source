import os

from freesound_api_key import FREESOUND_CLIENT_ID
from freesound_interface import logout_from_freesound, is_logged_in, get_currently_logged_in_user
from helpers import justify_text, frame_from_lines, PlStateNames, sound_parameters_info_dict, add_meter_to_frame, \
    add_voice_grid_to_frame

from .states_base import PaginatedState, ChangePresetOnEncoderShiftRotatedStateMixin, \
    GoBackOnEncoderLongPressedStateMixin, MenuState, EnterTextViaHWOrWebInterfaceState, MenuCallbackState, \
    EnterNumberState, EnterNoteState
from .states import ReverbSettingsMenuState, NewPresetOptionsMenuState, NewSoundOptionsMenuState, InfoPanelState
from .constants import EXTRA_PAGE_1_NAME, EXTRA_PAGE_2_NAME, sound_parameter_pages, note_layout_types


class HomeState(ChangePresetOnEncoderShiftRotatedStateMixin, PaginatedState):
    pages = [EXTRA_PAGE_2_NAME] + sound_parameter_pages + [EXTRA_PAGE_1_NAME]

    def draw_display_frame(self):
        n_sounds = self.spi.get_num_loaded_sounds()
        n_sounds_loaded_in_sampler = self.spi.get_property(PlStateNames.NUM_SOUNDS_LOADED_IN_SAMPLER, 0)

        text = "{0} sounds".format(n_sounds)
        if n_sounds_loaded_in_sampler < n_sounds:
            text += " ({} loading)".format(n_sounds - n_sounds_loaded_in_sampler)
        lines = [{
            "underline": True,
            "text": text,
        }]

        if self.current_page_data == EXTRA_PAGE_1_NAME:
            # Show some sound information
            lines += [
                justify_text('Temp:', '{0}ºC'.format(self.spi.get_property(PlStateNames.SYSTEM_STATS).get("temp", ""))),
                justify_text('Memory:', '{0}%'.format(self.spi.get_property(PlStateNames.SYSTEM_STATS).get("mem", ""))),
                justify_text('CPU:', '{0}% | {1:.1f}%'.format(self.spi.get_property(PlStateNames.SYSTEM_STATS).get("cpu", ""),
                                                              self.spi.get_property(PlStateNames.SYSTEM_STATS).get(
                                                                  "xenomai_cpu", 0.0))),
                justify_text('Network:',
                             '{0}'.format(self.spi.get_property(PlStateNames.SYSTEM_STATS).get("network_ssid", "-")))
            ]
        elif self.current_page_data == EXTRA_PAGE_2_NAME:
            # Show some volatile state informaion (meters and voice activations)
            lines += ['L:', 'R:']
        else:
            # Show page parameter values
            for parameter_name in self.current_page_data:
                if parameter_name is not None:
                    _, get_func, parameter_label, value_label_template, _ = sound_parameters_info_dict[parameter_name]

                    # Check if all loaded sounds have the same value for that parameter. If that is the case, show the
                    # value, otherwise don't show any value
                    all_sounds_values = []
                    last_value = None
                    for sound_idx in range(0, self.spi.get_property(PlStateNames.NUM_SOUNDS, 0)):
                        raw_value = self.spi.get_sound_property(sound_idx, parameter_name)
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
            frame = add_meter_to_frame(frame, y_offset_lines=3, value=self.spi.get_property(PlStateNames.METER_R, 0))
            frame = add_meter_to_frame(frame, y_offset_lines=2, value=self.spi.get_property(PlStateNames.METER_L, 0))
            voice_sound_idxs = [self.spi.get_source_sound_idx_from_source_sampler_sound_uuid(source_sampler_sound_uuid) for
                                source_sampler_sound_uuid in self.spi.get_property(PlStateNames.VOICE_SOUND_IDXS, [])]
            if voice_sound_idxs:
                frame = add_voice_grid_to_frame(frame, voice_activations=voice_sound_idxs)
            return frame  # Return before adding scrollbar if we're in first page
        return self.draw_scroll_bar(frame)

    def on_activating_state(self):
        self.sm.unset_all_leds()

    def on_button_pressed(self, button_idx, shift=False):
        # Select a sound
        sound_idx = self.get_sound_idx_from_buttons(button_idx, shift=shift)
        if sound_idx > -1:
            self.sm.move_to_selected_sound_state(sound_idx)

    def on_encoder_pressed(self, shift=False):
        if self.sm.should_show_start_animation is True:
            # Stop start animation
            self.sm.should_show_start_animation = False
        else:
            # Open home contextual menu
            self.sm.move_to(HomeContextualMenuState())

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
                if shift and parameter_name == "pitch" or shift and parameter_name == "gain" or shift and \
                        parameter_name == "mod2PitchAmt":
                    send_value = send_value * 0.3333333  # Reduced range mode
                self.spi.send_msg_to_plugin(osc_address, ["", parameter_name, send_value])


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
    OPTION_MIDI_IN_CHANNEL = "Set MIDI in ch..."
    OPTION_LOGIN_TO_FREESOUND = "Login to Freesound"
    OPTION_LOGOUT_FROM_FREESOUND = "Logout from Freesound"
    OPTION_DOWNLOAD_ORIGINAL = "Use original files..."
    OPTION_ABOUT = "About"

    items = [OPTION_SAVE, OPTION_SAVE_AS, OPTION_RELOAD, OPTION_LOAD_PRESET, \
             OPTION_NEW_SOUNDS, OPTION_ADD_NEW_SOUND, OPTION_RELAYOUT, OPTION_REVERB, \
             OPTION_NUM_VOICES, OPTION_GO_TO_SOUND, OPTION_SOUND_USAGE_LOG, OPTION_DOWNLOAD_ORIGINAL,
             OPTION_MIDI_IN_CHANNEL, OPTION_LOGIN_TO_FREESOUND, OPTION_ABOUT]
    page_size = 5

    def draw_display_frame(self):
        if is_logged_in() and self.OPTION_LOGOUT_FROM_FREESOUND not in self.items:
            self.items = self.items[:-2] + [self.OPTION_LOGOUT_FROM_FREESOUND] + [
                self.items[-1]]  # Keep OPTION_ABOUT the last option
        if not is_logged_in() and self.OPTION_LOGOUT_FROM_FREESOUND in self.items:
            self.items = [item for item in self.items if item != self.OPTION_LOGOUT_FROM_FREESOUND]

        lines = self.get_menu_item_lines()

        # Add FS username
        for line in lines:
            if line['text'] == self.OPTION_LOGOUT_FROM_FREESOUND:
                line['text'] = 'Logout ({})'.format(get_currently_logged_in_user())

        return frame_from_lines([self.get_default_header_line()] + lines)

    def get_exising_presets_list(self):
        # Scan existing .xml files to know preset names
        # TODO: the presetting system should be imporved so we don't need to scan the presets folder every time
        preset_names = {}
        presets_folder = self.spi.get_property(PlStateNames.PRESETS_DATA_LOCATION)
        if presets_folder is not None:
            for filename in os.listdir(presets_folder):
                if filename.endswith('.xml'):
                    try:
                        preset_id = int(filename.split('.xml')[0])
                    except ValueError:
                        # Not a valid preset file
                        continue
                    file_contents = open(os.path.join(presets_folder, filename), 'r').read()
                    preset_name = file_contents.split(' name="')[1].split('"')[0]
                    preset_names[preset_id] = preset_name
        return preset_names

    def move_to_choose_preset_name_state(self, preset_idx_name):
        preset_idx = int(preset_idx_name.split(':')[0])
        current_preset_name = current_preset_index = self.spi.get_property(PlStateNames.LOADED_PRESET_NAME,
                                                                      "")  # preset_idx_name.split(':')[1]
        self.sm.move_to(EnterTextViaHWOrWebInterfaceState(
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
            self.sm.go_back()
        elif action_name == self.OPTION_SAVE_AS:
            preset_names = self.get_exising_presets_list()
            current_preset_index = self.spi.get_property(PlStateNames.LOADED_PRESET_INDEX, 0)
            if not preset_names:
                self.sm.show_global_message('Some error\noccurred...')
                self.sm.go_back()
                self.sm.move_to(
                    EnterNumberState(initial=current_preset_index, minimum=0, maximum=127, title1="Load preset...",
                                     callback=self.load_preset, go_back_n_times=2))
            else:
                self.sm.move_to(MenuCallbackState(
                    items=['{0}:{1}'.format(i, preset_names.get(i, 'empty')) for i in range(0, 128)],
                    selected_item=current_preset_index,
                    title1="Where to save...",
                    callback=self.move_to_choose_preset_name_state,
                    go_back_n_times=0))

        elif action_name == self.OPTION_RELOAD:
            self.reload_current_preset()
            self.sm.go_back()
        elif action_name == self.OPTION_RELAYOUT:
            # see NOTE_MAPPING_TYPE_CONTIGUOUS in defines.h, contiguous and interleaved indexes must match with those
            # in that file (0 and 1)
            self.sm.move_to(MenuCallbackState(items=note_layout_types, selected_item=0, title1="Apply note layout...",
                                         callback=self.reapply_note_layout, go_back_n_times=2))
        elif action_name == self.OPTION_NUM_VOICES:
            current_num_voices = self.spi.get_property(PlStateNames.NUM_VOICES, 1)
            self.sm.move_to(EnterNumberState(initial=current_num_voices, minimum=1, maximum=32, title1="Number of voices",
                                        callback=self.set_num_voices, go_back_n_times=2))
        elif action_name == self.OPTION_LOAD_PRESET:
            preset_names = self.get_exising_presets_list()
            current_preset_index = self.spi.get_property(PlStateNames.LOADED_PRESET_INDEX, 0)
            if not preset_names:
                self.sm.move_to(
                    EnterNumberState(initial=current_preset_index, minimum=0, maximum=127, title1="Load preset...",
                                     callback=self.load_preset, go_back_n_times=2))
            else:
                self.sm.move_to(
                    MenuCallbackState(items=['{0}:{1}'.format(i, preset_names.get(i, 'empty')) for i in range(0, 128)],
                                      selected_item=current_preset_index, title1="Load preset...",
                                      callback=self.load_preset, go_back_n_times=2))
        elif action_name == self.OPTION_REVERB:
            self.sm.move_to(ReverbSettingsMenuState())
        elif action_name == self.OPTION_ADD_NEW_SOUND:
            self.sm.move_to(NewSoundOptionsMenuState())
        elif action_name == self.OPTION_NEW_SOUNDS:
            self.sm.move_to(NewPresetOptionsMenuState())
        elif action_name == self.OPTION_GO_TO_SOUND:
            self.sm.move_to(EnterNoteState(
                title1="Go to sound...",
                callback=lambda note: (
                self.sm.go_back(n_times=2), self.sm.move_to_selected_sound_state(self.get_sound_idx_from_note(note))),
            ))
        elif action_name == self.OPTION_SOUND_USAGE_LOG:
            self.sm.show_global_message('Sound usage log\n opened in browser')
            self.sm.open_url_in_browser = '/usage_log'
            self.sm.go_back()
        elif action_name == self.OPTION_LOGIN_TO_FREESOUND:
            self.sm.show_global_message('Check login\nin browser')
            self.sm.open_url_in_browser = \
                'https://freesound.org/apiv2/oauth2/logout_and_authorize/?client_id={}&response_type=code'.format(
                FREESOUND_CLIENT_ID)
            self.sm.go_back()
        elif action_name == self.OPTION_LOGOUT_FROM_FREESOUND:
            logout_from_freesound()
            self.sm.show_global_message('Logged out\nfrom Freesound')
            self.sm.go_back()
        elif action_name == self.OPTION_DOWNLOAD_ORIGINAL:
            selected_item = ['never', 'onlyShort', 'always'].index(
                self.spi.get_property(PlStateNames.USE_ORIGINAL_FILES_PREFERENCE, 'never'))
            self.sm.move_to(MenuCallbackState(items=['Never', 'Only small', 'Always'], selected_item=selected_item,
                                         title1="Use original files...",
                                         callback=lambda x: self.set_download_original_files(x), go_back_n_times=2))
        elif action_name == self.OPTION_MIDI_IN_CHANNEL:
            current_midi_in = self.spi.get_property(PlStateNames.MIDI_IN_CHANNEL, 0)
            self.sm.move_to(EnterNumberState(initial=current_midi_in, minimum=0, maximum=16, title1="Midi in channel",
                                        callback=self.set_midi_in_chhannel, go_back_n_times=2))
        elif action_name == self.OPTION_ABOUT:
            try:
                last_commit_info = open('last_commit_info', 'r').readlines()[0]
                commit_hash = last_commit_info.split(' ')[0]
                commit_date = '{} {}'.format(last_commit_info.split(' ')[1], last_commit_info.split(' ')[2][0:5])
            except:
                commit_hash = '-'
                commit_date = '-'
            plugin_version = self.spi.get_property(PlStateNames.PLUGIN_VERSION, '0.0')
            self.sm.move_to(InfoPanelState(title='About', lines=[
                justify_text('Plugin version:', '{}'.format(plugin_version)),
                justify_text('Commit:', '{}'.format(commit_hash)),
                justify_text('Date:', '{}'.format(commit_date)),
            ]))
        else:
            self.sm.show_global_message('Not implemented...')
