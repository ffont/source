import math
import os
import random
import time

import numpy
from scipy.io import wavfile

from freesound_interface import is_logged_in, get_user_bookmarks
from helpers import justify_text, frame_from_lines, PlStateNames, add_scroll_bar_to_frame, \
    add_centered_value_to_frame, add_sound_waveform_and_extras_to_frame, DISPLAY_SIZE, \
    add_midi_keyboard_and_extras_to_frame, merge_dicts, \
    raw_assigned_notes_to_midi_assigned_notes, add_recent_query_filter, \
    sound_parameters_info_dict, get_recent_query_filters, get_filenames_in_dir, clear_moving_text_cache

from .states_base import PaginatedState, GoBackOnEncoderLongPressedStateMixin, ShowHelpPagesMixin, MenuState, \
    EnterTextViaHWOrWebInterfaceState, MenuCallbackState, State
from .constants import query_settings_pages, midi_cc_available_parameters_list, predefined_queries, \
    reverb_parameters_pages, query_settings_info_dict, note_layout_types, reverb_parameters_info_dict


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
                reverb_params = self.spi.get_property(PlStateNames.REVERB_SETTINGS, [0.0, 0.0, 0.0, 0.0, 0.0, 0.0])
                lines.append(justify_text(
                    parameter_label + ":",
                    '{0:.2f}'.format(reverb_params[parameter_position])
                ))
            else:
                # Add blank line
                lines.append("")

        return self.draw_scroll_bar(frame_from_lines([self.get_default_header_line()] + lines))

    def on_encoder_pressed(self, shift=False):
        self.sm.go_back()

    def on_fader_moved(self, fader_idx, value, shift=False):
        parameter_name = self.current_page_data[fader_idx]
        if parameter_name is not None:
            param_position, _ = reverb_parameters_info_dict[parameter_name]
            reverb_params = self.spi.get_property(PlStateNames.REVERB_SETTINGS, [0.0, 0.0, 0.0, 0.0, 0.0, 0.0])
            reverb_params[param_position] = value
            self.spi.send_msg_to_plugin("/set_reverb_parameters", reverb_params)


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
        self.query_settings_values['num_sounds'] = kwargs.get('num_sounds', 8)
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
                lines += [justify_text(label + ':', value_template.format(value) if self.parameter_is_allowed(
                    parameter_name) else '-')]
            else:
                lines += [""]

        return self.draw_scroll_bar(frame_from_lines([self.get_default_header_line()] + lines))

    def on_encoder_pressed(self, shift=False):
        # Move to query chooser state
        if self.callback is not None:
            query_settings = self.query_settings_values.copy()
            query_settings.update(
                {'layout': note_layout_types.index(query_settings['layout'])})  # Change the format of layout parameter
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
                self.sm.show_global_message('Loaded recent\n qeury filters ({})'.format(
                    self.last_recent_loaded % len(recent_query_filters) + 1), duration=0.5)
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
            self.sm.move_to(EnterQuerySettingsState(
                callback=lambda query_settings: self.sm.move_to(
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
            self.sm.move_to(EnterQuerySettingsState(
                callback=lambda query_settings: self.sm.move_to(
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
            self.sm.move_to(EnterQuerySettingsState(
                callback=lambda query_settings: (
                self.new_preset_by_random_sounds(**query_settings), self.sm.go_back(n_times=3))
            ))
        elif action_name == self.OPTION_BY_SIMILARITY:
            current_note_mapping_type = self.spi.get_property(PlStateNames.NOTE_LAYOUT_TYPE, 1)
            self.new_preset_by_similar_sounds(note_mapping_type=current_note_mapping_type)
            self.sm.go_back(n_times=3)
        else:
            self.sm.show_global_message('Not implemented...')


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
            self.sm.move_to(EnterQuerySettingsState(
                callback=lambda query_settings:
                self.sm.move_to(
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
            self.sm.move_to(EnterQuerySettingsState(
                callback=lambda query_settings: (
                self.add_or_replace_sound_random(**merge_dicts(query_settings, {'move_once_loaded': True})),
                self.sm.go_back(n_times=3)),
                num_sounds=1,
                allow_change_num_sounds=False,
                allow_change_layout=False
            ))
        elif action_name == self.OPTION_LOCAL:
            base_path = self.spi.get_local_audio_files_path()
            if base_path is not None:
                available_files_ogg, _, _ = get_filenames_in_dir(base_path, '*.ogg')
                available_files_wav, _, _ = get_filenames_in_dir(base_path, '*.wav')
                available_files = sorted(available_files_ogg + available_files_wav)
                self.sm.move_to(FileChooserState(
                    base_path=base_path,
                    items=available_files,
                    title1="Select a file...",
                    go_back_n_times=3,
                    callback=lambda file_path: (self.send_add_or_replace_sound_to_plugin(
                        "",  # "" to add new sound
                        {
                            'id': -1,
                            'name': file_path.split('/')[-1],
                            'username': '-',
                            'license': '-',
                            'type': file_path.split('.')[1],
                            'filesize': os.path.getsize(file_path),
                            'previews': {'preview-hq-ogg': ''},
                        },
                        local_file_path=file_path,
                        move_once_loaded=True
                    ), self.sm.show_global_message('Loading file\n{}...'.format(file_path.split('/')[-1]))),
                ))
            else:
                self.sm.show_global_message('Local files\nnot ready...')
        elif action_name == self.OPTION_FROM_BOOKMARK:
            self.sm.show_global_message('Getting\nbookmarks...')
            user_bookmarks = get_user_bookmarks()
            self.sm.show_global_message('')
            sounds_data = {item['name']: item for item in user_bookmarks}
            self.sm.move_to(SoundChooserState(
                items=[item['name'] for item in user_bookmarks],
                sounds_data=sounds_data,
                title1="Select a sound...",
                go_back_n_times=3,
                callback=lambda sound_name: (self.send_add_or_replace_sound_to_plugin(
                    "",  # "" to add new sound
                    sounds_data[sound_name],
                    move_once_loaded=True
                ), self.sm.show_global_message('Loading file\n{}...'.format(sound_name))),
            ))


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
            "text": "S{0}:{1}".format(self.sound_idx + 1, self.spi.get_sound_property(self.sound_idx, PlStateNames.SOUND_NAME))
        }, {
            "underline": True,
            "text": "Replace by..."
        }]
        lines += self.get_menu_item_lines()
        return frame_from_lines([self.get_default_header_line()] + lines)

    def perform_action(self, action_name):
        if action_name == self.OPTION_BY_QUERY:
            self.sm.move_to(EnterQuerySettingsState(
                callback=lambda query_settings: self.sm.move_to(
                    EnterTextViaHWOrWebInterfaceState(
                        title="Enter query",
                        web_form_id="enterQuery",
                        callback=self.add_or_replace_sound_by_query,
                        extra_data_for_callback=merge_dicts(query_settings, {
                            'sound_uuid': self.spi.get_sound_property(self.sound_idx, PlStateNames.SOUND_UUID, '-')}),
                        go_back_n_times=4
                    )),
                num_sounds=1,
                allow_change_num_sounds=False,
                allow_change_layout=False
            ))
        elif action_name == self.OPTION_BY_SIMILARITY:
            selected_sound_id = self.spi.get_sound_property(self.sound_idx, PlStateNames.SOUND_ID)
            if selected_sound_id != '-':
                self.replace_sound_by_similarity(self.spi.get_sound_property(self.sound_idx, PlStateNames.SOUND_UUID, '-'),
                                                 selected_sound_id)
                self.sm.go_back(n_times=2)  # Go back 2 times because option is 2-levels deep in menu hierarchy
        elif action_name == self.OPTION_BY_RANDOM:
            self.sm.move_to(EnterQuerySettingsState(
                callback=lambda query_settings: (self.add_or_replace_sound_random(**merge_dicts(query_settings, {
                    'sound_uuid': self.spi.get_sound_property(self.sound_idx, PlStateNames.SOUND_UUID, '-')})),
                                                 self.sm.go_back(n_times=3)),
                num_sounds=1,
                allow_change_num_sounds=False,
                allow_change_layout=False
            ))
        elif action_name == self.OPTION_FROM_DISK:
            base_path = self.spi.get_local_audio_files_path()
            if base_path is not None:
                available_files_ogg, _, _ = get_filenames_in_dir(base_path, '*.ogg')
                available_files_wav, _, _ = get_filenames_in_dir(base_path, '*.wav')
                available_files = sorted(available_files_ogg + available_files_wav)
                self.sm.move_to(FileChooserState(
                    base_path=base_path,
                    items=available_files,
                    title1="Select a file...",
                    go_back_n_times=3,
                    callback=lambda file_path: (self.send_add_or_replace_sound_to_plugin(
                        self.spi.get_sound_property(self.sound_idx, PlStateNames.SOUND_UUID, '-'),
                        {
                            'id': -1,
                            'name': file_path.split('/')[-1],
                            'username': '-',
                            'license': '-',
                            'type': file_path.split('.')[1],
                            'filesize': os.path.getsize(file_path),
                            'previews': {'preview-hq-ogg': ''},
                        },
                        local_file_path=file_path
                    ), self.sm.show_global_message('Loading file\n{}...'.format(file_path.split('/')[-1]))),
                ))
            else:
                self.sm.show_global_message('Local files\nnot ready...')
        elif action_name == self.OPTION_FROM_BOOKMARK:
            self.sm.show_global_message('Getting\nbookmarks...')
            user_bookmarks = get_user_bookmarks()
            self.sm.show_global_message('')
            sounds_data = {item['name']: item for item in user_bookmarks}
            self.sm.move_to(SoundChooserState(
                items=[item['name'] for item in user_bookmarks],
                sounds_data=sounds_data,
                title1="Select a sound...",
                go_back_n_times=3,
                callback=lambda sound_name: (self.send_add_or_replace_sound_to_plugin(
                    self.spi.get_sound_property(self.sound_idx, PlStateNames.SOUND_UUID, '-'),
                    sounds_data[sound_name]
                ), self.sm.show_global_message('Loading file\n{}...'.format(sound_name))),
            ))

        else:
            self.sm.show_global_message('Not implemented...')


class EditMIDICCAssignmentState(GoBackOnEncoderLongPressedStateMixin, State):
    cc_number = -1
    parameter_name = ""
    min_range = 0.0
    max_range = 1.0
    uuid = ""
    sound_idx = -1
    available_parameter_names = midi_cc_available_parameters_list

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.cc_number = kwargs.get('cc_number', -1)  # Default to MIDI learn mode
        self.parameter_name = kwargs.get('parameter_name', self.available_parameter_names[-1])
        self.min_range = kwargs.get('min_range', 0.0)
        self.max_range = kwargs.get('max_range', 1.0)
        self.uuid = kwargs.get('uuid', -1)
        self.sound_idx = kwargs.get('sound_idx', -1)

    def draw_display_frame(self):
        if self.cc_number > -1:
            cc_number = str(self.cc_number)
        else:
            last_cc_received = int(self.spi.get_property(PlStateNames.LAST_CC_MIDI_RECEIVED, 0))
            if last_cc_received < 0:
                last_cc_received = 0
            cc_number = 'MIDI Learn ({0})'.format(last_cc_received)

        lines = [{
            "underline": True,
            "text": "Edit MIDI CC mappaing"
        },
            justify_text('Param:', sound_parameters_info_dict[self.parameter_name][2]),
            # Show parameter label instead of raw name
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
            last_cc_received = int(self.spi.get_property(PlStateNames.LAST_CC_MIDI_RECEIVED, 0))
            if last_cc_received < 0:
                last_cc_received = 0
            cc_number = last_cc_received
        self.spi.send_msg_to_plugin('/add_or_update_cc_mapping',
                               [self.spi.get_sound_property(self.sound_idx, PlStateNames.SOUND_UUID, '-'), self.uuid,
                                cc_number, self.parameter_name, self.min_range, self.max_range])
        self.sm.show_global_message('Adding MIDI\nmapping...')
        self.sm.go_back()

    def on_fader_moved(self, fader_idx, value, shift=False):
        if fader_idx == 0:
            self.parameter_name = self.available_parameter_names[int(value * (len(self.available_parameter_names) - 1))]
        elif fader_idx == 1:
            self.cc_number = int(value * 128) - 1
        elif fader_idx == 2:
            self.min_range = value
        elif fader_idx == 3:
            self.max_range = value


class SoundSliceEditorState(ShowHelpPagesMixin, GoBackOnEncoderLongPressedStateMixin, State):

    frame_count = 0
    sound_idx = -1
    sound_data_array = None  # numpy array of shape (x, 1)
    cursor_position = 0  # In samples
    sound_length = 0  # In samples
    sound_sr = 44100  # In samples
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
    ], [
        "B5 add slice",
        "B6 remove closest slice",
        "B7 remove all slices"
    ], [
        "F1 zoom",
        "F2 scale",
        "F3 cursor"
    ], [
        "E save and exit",
        "E+S save"
    ]]

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.sound_idx = kwargs.get('sound_idx', -1)

    def on_activating_state(self):
        if self.sound_idx > -1:
            # The plugin saves .wav versions of all the loaded sounds in a TMP location and with a known filename
            # Here we use these files to load them directly in the slice editor and there is no need to send the
            # files over the websockets or osc connection
            path = os.path.join(self.spi.get_property(PlStateNames.TMP_DATA_LOCATION, ''),
                                self.spi.get_sound_property(self.sound_idx,
                                                        PlStateNames.SOURCE_SAMPLER_SOUND_UUID) + '.wav')
            if os.path.exists(path):
                self.sm.show_global_message('Loading\nwaveform...', duration=10)
                self.sound_sr, self.sound_data_array = wavfile.read(path)
                if self.sound_data_array.dtype == numpy.uint8:
                    # if type is uint8, change to int8 so it is centered at 0 and not 128
                    self.sound_data_array = self.sound_data_array.view(numpy.int8)
                    self.sound_data_array -= 128                    
                if len(self.sound_data_array.shape) > 1:
                    self.sound_data_array = self.sound_data_array[:, 0]  # Take 1 channel only
                self.sound_data_array = self.sound_data_array / abs(
                    max(self.sound_data_array.min(), self.sound_data_array.max(), key=abs))  # Normalize
                self.slices = [int(s * self.sound_sr) for s in
                                self.spi.get_sound_property(self.sound_idx, PlStateNames.SOUND_SLICES, default=[])]
                self.sound_length = self.sound_data_array.shape[0]
                self.min_zoom = (self.sound_length // self.display_width) + 1
                self.current_zoom = self.min_zoom
                audio_mean_value = numpy.absolute(self.sound_data_array).mean()
                if audio_mean_value < 0.01:
                    # If the file contains very low energy, auto-scale it a bit
                    self.scale = 10
                self.sm.show_global_message('', duration=0)
            else:
                self.sm.show_global_message('File not found')

    def draw_display_frame(self):
        self.frame_count += 1
        lines = [{
            "underline": True,
            "move": True,
            "text": "S{0}:{1}".format(self.sound_idx + 1, self.spi.get_sound_property(self.sound_idx, PlStateNames.SOUND_NAME))
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

            start_position = int(
                float(self.spi.get_sound_property(self.sound_idx, 'startPosition', default=0.0)) * self.sound_length)
            end_position = int(
                float(self.spi.get_sound_property(self.sound_idx, 'endPosition', default=1.0)) * self.sound_length)
            loop_start_position = int(
                float(self.spi.get_sound_property(self.sound_idx, 'loopStartPosition', default=0.0)) * self.sound_length)
            loop_end_position = int(
                float(self.spi.get_sound_property(self.sound_idx, 'loopEndPosition', default=1.0)) * self.sound_length)

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
        self.set_slices_for_sound(self.spi.get_sound_property(self.sound_idx, PlStateNames.SOUND_UUID, '-'), time_slices)
        self.sm.show_global_message('Updated {} slices'.format(len(time_slices)))
        if not shift:
            # If shift is not pressed, go back to sound selected state
            self.sm.go_back(n_times=2)

    def on_button_pressed(self, button_idx, shift=False):
        if button_idx == 1:
            # Set start position
            self.spi.send_msg_to_plugin('/set_sound_parameter',
                                   [self.spi.get_sound_property(self.sound_idx, PlStateNames.SOUND_UUID, '-'), "startPosition",
                                    self.cursor_position * 1.0 / self.sound_length])
        elif button_idx == 2:
            # Set loop start position
            self.spi.send_msg_to_plugin('/set_sound_parameter',
                                   [self.spi.get_sound_property(self.sound_idx, PlStateNames.SOUND_UUID, '-'),
                                    "loopStartPosition", self.cursor_position * 1.0 / self.sound_length])
        elif button_idx == 3:
            # Set loop end position
            self.spi.send_msg_to_plugin('/set_sound_parameter',
                                   [self.spi.get_sound_property(self.sound_idx, PlStateNames.SOUND_UUID, '-'),
                                    "loopEndPosition", self.cursor_position * 1.0 / self.sound_length])
        elif button_idx == 4:
            # Set end position
            self.spi.send_msg_to_plugin('/set_sound_parameter',
                                   [self.spi.get_sound_property(self.sound_idx, PlStateNames.SOUND_UUID, '-'), "endPosition",
                                    self.cursor_position * 1.0 / self.sound_length])
        elif button_idx == 5:
            # Add slice
            if self.cursor_position not in self.slices:
                self.slices = sorted(self.slices + [self.cursor_position])
        elif button_idx == 6:
            # Remove closest slice
            if self.slices:
                slice_to_remove_idx = min(range(len(self.slices)),
                                          key=lambda i: abs(self.slices[i] - self.cursor_position))
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
            self.scale = (((value - 0) * (self.max_scale - self.min_scale)) / 1) + self.min_scale
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
    ], [
        "B4 clear all",
        "B5 set all",
        "B6 set root to sel",
        "B7 set root to last r",
    ], [
        "E save and exit",
        "E+S save",
        "EE se and u others",
        "EE+S s and u others",
    ]]

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.sound_idx = kwargs.get('sound_idx', -1)

    def on_activating_state(self):
        if self.sound_idx > -1:
            sound_assigned_notes = self.spi.get_sound_property(self.sound_idx, PlStateNames.SOUND_ASSIGNED_NOTES)
            if sound_assigned_notes is not None:
                self.root_note = int(self.spi.get_sound_property(self.sound_idx, "midiRootNote", default=-1))
                self.assigned_notes = raw_assigned_notes_to_midi_assigned_notes(sound_assigned_notes)
                if self.assigned_notes:
                    if self.root_note > -1:
                        self.cursor_position = self.root_note
                    else:
                        self.cursor_position = self.assigned_notes[0]

    def save_assigned_notes(self, unassign_from_others=False):
        self.set_assigned_notes_for_sound(self.spi.get_sound_property(self.sound_idx, PlStateNames.SOUND_UUID, '-'),
                                          self.assigned_notes, root_note=self.root_note)
        if unassign_from_others:
            # Iterate over all sounds and make sure the notes for the current sound are not assigned
            # to any other sound
            for sound_idx in range(0, self.sm.get_num_loaded_sounds()):
                if self.spi.get_sound_property(sound_idx, PlStateNames.SOUND_UUID) != \
                        self.spi.get_sound_property(self.sound_idx, PlStateNames.SOUND_UUID, '-'):
                    sound_assigned_notes = self.spi.get_sound_property(sound_idx, PlStateNames.SOUND_ASSIGNED_NOTES)
                    if sound_assigned_notes is not None:
                        midi_notes = raw_assigned_notes_to_midi_assigned_notes(sound_assigned_notes)
                        new_midi_notes = [note for note in midi_notes if note not in self.assigned_notes]
                        if len(midi_notes) != len(new_midi_notes):
                            self.set_assigned_notes_for_sound(self.spi.get_sound_property(sound_idx, PlStateNames.SOUND_UUID),
                                                              new_midi_notes,
                                                              root_note=int(
                                                                  self.spi.get_sound_property(sound_idx, "midiRootNote",
                                                                                         default=-1)))

        self.sm.show_global_message(
            'Updated {}\n assigned notes{}'.format(len(self.assigned_notes), ' (U)' if unassign_from_others else ''))

    def draw_display_frame(self):
        lines = [{
            "underline": True,
            "move": True,
            "text": "S{0}:{1}".format(self.sound_idx + 1, self.spi.get_sound_property(self.sound_idx, PlStateNames.SOUND_NAME))
        }]
        frame = frame_from_lines([self.get_default_header_line()] + lines)
        self.frame_count += 1
        if self.range_start is not None:
            currently_selected_range = list(
                range(min(self.range_start, self.cursor_position), max(self.range_start, self.cursor_position) + 1))
        else:
            currently_selected_range = []

        current_last_note_from_state = self.spi.get_property(PlStateNames.LAST_NOTE_MIDI_RECEIVED, -1)
        if current_last_note_from_state != self.last_note_from_state or self.spi.get_property(PlStateNames.MIDI_RECEIVED,
                                                                                         False):
            # A new note was triggered
            self.last_note_from_state = current_last_note_from_state
            self.last_note_updated_time = time.time()

        if time.time() - self.last_note_updated_time < self.hold_last_note:
            last_note_received = self.last_note_from_state
            # This is used for the "assign root note to last received" function
            self.last_note_received_persistent = last_note_received
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
            self.sm.go_back(n_times=2)

    def on_encoder_double_pressed(self, shift):
        # Save new assigned notes and remove notes from other sounds
        self.save_assigned_notes(unassign_from_others=True)
        if not shift:
            # If shift is not pressed, go back to sound selected state
            self.sm.go_back(n_times=2)

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
                        self.assigned_notes = [note for note in self.assigned_notes if
                                               note != self.last_note_received_persistent]
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
                    self.assigned_notes = sorted(list(set(self.assigned_notes + list(
                        range(min(self.range_start, self.cursor_position),
                              max(self.range_start, self.cursor_position) + 1)))))
                    self.range_start = None
                self.selecting_range_mode = False
        elif button_idx == 4:
            # Clear all assigned notes
            self.assigned_notes = []
        elif button_idx == 5:
            # Sett all assigned notes
            self.assigned_notes = list(range(0, 128))
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
        self.shift_callback = lambda file_path: self.spi.send_msg_to_plugin('/play_sound_from_path', [
            file_path])  # Send preview sound OSC on shift+encoder
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
        self.spi.send_msg_to_plugin('/play_sound_from_path', [""])  # Stop sound being previewed (if any)

    def on_deactivating_state(self):
        super().on_deactivating_state()
        self.spi.send_msg_to_plugin('/play_sound_from_path', [""])  # Stop sound being previewed (if any)


class SoundChooserState(FileChooserState):
    base_path = ''
    sounds_data = {}

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.sounds_data = kwargs.get('sounds_data', '')
        # Send preview sound OSC on shift+encoder
        self.shift_callback = lambda sound_name: self.spi.send_msg_to_plugin('/play_sound_from_path', [
            self.sounds_data.get(sound_name, {}).get("previews", {}).get("preview-lq-ogg",
                                                                         "")])


class InfoPanelState(GoBackOnEncoderLongPressedStateMixin, State):

    def __init__(self, title='title', lines=[]):
        self.lines = lines
        self.title = title

    def draw_display_frame(self):
        lines = [{
            "underline": True,
            "text": self.title
        }]
        lines += self.lines
        return frame_from_lines([self.get_default_header_line()] + lines)
