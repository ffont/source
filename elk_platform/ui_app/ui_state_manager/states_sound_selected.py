import os

from freesound_interface import is_logged_in, bookmark_sound
from helpers import justify_text, frame_from_lines, PlStateNames, sound_parameters_info_dict, sizeof_fmt

from .states_base import PaginatedState, GoBackOnEncoderLongPressedStateMixin, MenuState, MenuCallbackState, \
    EnterNoteState
from .constants import EXTRA_PAGE_1_NAME, EXTRA_PAGE_2_NAME, sound_parameter_pages
from .states import ReplaceByOptionsMenuState, SoundSliceEditorState, SoundAssignedNotesEditorState, \
    EditMIDICCAssignmentState


class SoundSelectedState(GoBackOnEncoderLongPressedStateMixin, PaginatedState):
    pages = [EXTRA_PAGE_1_NAME, EXTRA_PAGE_2_NAME]
    sound_idx = -1
    selected_sound_is_playing = False

    def __init__(self, sound_idx, *args, **kwargs):
        self.sound_idx = sound_idx
        '''num_sounds = self.spi.get_property(PlStateNames.NUM_SOUNDS, 0)
        if sound_idx < 0:
            self.sound_idx = 0
        elif sound_idx >= num_sounds - 1:
            self.sound_idx = num_sounds - 1
        else:
            self.sound_idx = sound_idx

        if self.sound_is_loaded_in_sampler():
            self.pages += sound_parameter_pages'''
        super().__init__(*args, **kwargs)

    def sound_is_downloading(self):
        if self.spi.get_sound_property(self.sound_idx, PlStateNames.SOUND_OGG_URL, default=''):
            return self.spi.get_sound_property(self.sound_idx, PlStateNames.SOUND_DOWNLOAD_PROGRESS, default='0') != '100'
        else:
            # If sound does not have property SOUND_OGG_URL, then it is not a Freesound sound that
            # should be downloaded but a localfile, therefore it should be already in disk
            return False

    def sound_is_loaded_in_sampler(self):
        return self.spi.get_sound_property(self.sound_idx, PlStateNames.SOUND_LOADED_IN_SAMPLER, default=False)

    def get_properties(self):
        properties = super().get_properties().copy()
        properties.update({
            'sound_idx': self.sound_idx
        })
        return properties

    def draw_display_frame(self):
        num_sound_samples = self.spi.get_num_source_sampler_sounds_per_sound(self.sound_idx)
        if (num_sound_samples < 2):
            name_label = "S{0}: {1}".format(self.sound_idx + 1, self.spi.get_sound_property(self.sound_idx, PlStateNames.SOUND_NAME))
        else:
            name_label = "S{0} ({2} sounds): {1}".format(self.sound_idx + 1, self.spi.get_sound_property(self.sound_idx, PlStateNames.SOUND_NAME), num_sound_samples)
        
        lines = [{
            "underline": True,
            "move": True,
            "text": name_label
        }]

        if self.current_page_data == EXTRA_PAGE_1_NAME:
            # Show some sound information
            sound_id = self.spi.get_sound_property(self.sound_idx, PlStateNames.SOUND_ID)
            lines += [
                justify_text('ID:', '{0}'.format(sound_id) if sound_id != "-1" else "-"),
                justify_text('User:', '{0}'.format(self.spi.get_sound_property(self.sound_idx, PlStateNames.SOUND_AUTHOR))),
                justify_text('License:', '{0}'.format(self.spi.get_sound_property(self.sound_idx, PlStateNames.SOUND_LICENSE)))
            ]
            if self.sound_is_loaded_in_sampler():
                lines += [justify_text('Duration:', '{0:.2f}s'.format(
                    self.spi.get_sound_property(self.sound_idx, PlStateNames.SOUND_DURATION)))]
            else:
                sound_download_progress = int(
                    self.spi.get_sound_property(self.sound_idx, PlStateNames.SOUND_DOWNLOAD_PROGRESS, default=0))
                if self.sound_is_downloading():
                    if sound_download_progress < 95:
                        lines += [justify_text('Downloading...', '{0}%'.format(sound_download_progress))]
                    else:
                        lines += ['Loading in sampler...']
                else:
                    # If sound is not loaded in sampler and is not downloading it can be that:
                    # 1) sound comes from a local file (not from a Freesound download, even if it was previosuly
                    # downloaded) and is being loaded right now
                    # 2) sound comes from a local file (not from a Freesound download, even if it was previosuly
                    # downloaded) and it can't be found on disk (or there were errors loading)
                    if self.spi.get_sound_property(self.sound_idx, PlStateNames.SOUND_LOCAL_FILE_PATH,
                                              default='') and not os.path.exists(
                            self.spi.get_sound_property(self.sound_idx, PlStateNames.SOUND_LOCAL_FILE_PATH)):
                        lines += ['Local file not found...']
                    else:
                        # If there are errors loading, this will always show 'Loading in sampler...'
                        # This could be improved in the future
                        lines += ['Loading in sampler...']

        elif self.current_page_data == EXTRA_PAGE_2_NAME:
            # Show other sound information
            using_preview_marker = '*' if self.spi.get_sound_property(self.sound_idx,
                                                                 PlStateNames.SOUND_LOADED_PREVIEW_VERSION,
                                                                 default=True) and self.sound_is_loaded_in_sampler() \
                else ''
            lines += [
                justify_text('Type:', '{}{}'.format(
                    self.spi.get_sound_property(self.sound_idx, PlStateNames.SOUND_TYPE, default='-').upper(),
                    using_preview_marker)),
                justify_text('Size:', '{}{}'.format(
                    sizeof_fmt(self.spi.get_sound_property(self.sound_idx, PlStateNames.SOUND_FILESIZE, default=0)),
                    using_preview_marker))
            ]
        else:
            # Show page parameter values
            for parameter_name in self.current_page_data:
                if parameter_name is not None:
                    _, get_func, parameter_label, value_label_template, _ = sound_parameters_info_dict[parameter_name]
                    raw_value = self.spi.get_sound_property(self.sound_idx, parameter_name)
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
        self.spi.send_msg_to_plugin("/play_sound", [self.spi.get_sound_property(self.sound_idx, PlStateNames.SOUND_UUID, '-')])
        self.selected_sound_is_playing = True

    def stop_selected_sound(self):
        if self.selected_sound_is_playing:
            self.spi.send_msg_to_plugin("/stop_sound", [self.spi.get_sound_property(self.sound_idx, PlStateNames.SOUND_UUID, '-')])
            self.selected_sound_is_playing = False

    def on_activating_state(self):
        num_sounds = self.spi.get_property(PlStateNames.NUM_SOUNDS, 0)
        if self.sound_idx < 0:
            self.sound_idx = 0
        elif self.sound_idx >= num_sounds - 1:
            self.sound_idx = num_sounds - 1

        if self.sound_is_loaded_in_sampler():
            self.pages += sound_parameter_pages
        self.sm.set_led((self.sound_idx % 8) + 1, unset_others=True)

    def on_deactivating_state(self):
        self.stop_selected_sound()

    def on_source_state_update(self):
        # Check that self.sound_idx is in range with the new state, otherwise change the state to a new state with
        # valid self.sound_idx
        num_sounds = self.spi.get_property(PlStateNames.NUM_SOUNDS, 0)
        if num_sounds == 0:
            self.sm.go_back()
        else:
            if self.sound_idx >= num_sounds:
                self.sm.move_to(SoundSelectedState(num_sounds - 1, current_page=self.current_page), replace_current=True)
            elif self.sound_idx < 0 and num_sounds > 0:
                self.sm.move_to(SoundSelectedState(0, current_page=self.current_page), replace_current=True)
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
            self.sm.move_to(SoundSelectedState(sound_idx, current_page=self.current_page), replace_current=True)

    def on_encoder_rotated(self, direction, shift=False):
        if shift:
            # Stop current sound
            self.stop_selected_sound()

            # Select another sound
            num_sounds = self.spi.get_property(PlStateNames.NUM_SOUNDS, 0)
            if num_sounds:
                sound_idx = self.sound_idx + direction
                if sound_idx < 0:
                    sound_idx = num_sounds - 1
                if sound_idx >= num_sounds:
                    sound_idx = 0
                self.sm.move_to(SoundSelectedState(sound_idx, current_page=self.current_page), replace_current=True)
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
            self.sm.move_to(SoundSelectedContextualMenuState(sound_idx=self.sound_idx,
                                                        sound_finished_loading=self.sound_is_loaded_in_sampler()))
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
                self.spi.send_msg_to_plugin(osc_address, [self.spi.get_sound_property(self.sound_idx, PlStateNames.SOUND_UUID, '-'),
                                                     parameter_name, send_value])


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
    items = [OPTION_REPLACE, OPTION_MIDI_CC, OPTION_ASSIGNED_NOTES, OPTION_PRECISION_EDITOR, OPTION_OPEN_IN_FREESOUND,
             OPTION_DELETE, OPTION_GO_TO_SOUND]
    page_size = 4

    def __init__(self, sound_idx, sound_finished_loading=False, *args, **kwargs):
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
        # Check that self.sound_idx is in range with the new state, otherwise change the state to a new state with
        # valid self.sound_idx
        num_sounds = self.spi.get_property(PlStateNames.NUM_SOUNDS, 0)
        if self.sound_idx >= num_sounds:
            self.sm.move_to(SoundSelectedContextualMenuState(num_sounds - 1), replace_current=True)
        elif self.sound_idx < 0 and num_sounds > 0:
            self.sm.move_to(SoundSelectedContextualMenuState(0), replace_current=True)

    def draw_display_frame(self):
        if is_logged_in() and self.OPTION_BOOKMARK not in self.items:
            self.items.append(self.OPTION_BOOKMARK)
        if not is_logged_in() and self.OPTION_BOOKMARK in self.items:
            self.items = [item for item in self.items if item != self.OPTION_BOOKMARK]

        lines = [{
            "underline": True,
            "move": True,
            "text": "S{0}:{1}".format(self.sound_idx + 1, self.spi.get_sound_property(self.sound_idx, PlStateNames.SOUND_NAME))
        }]
        lines += self.get_menu_item_lines()
        return frame_from_lines([self.get_default_header_line()] + lines)

    def perform_action(self, action_name):
        if action_name == self.OPTION_REPLACE:
            self.sm.move_to(ReplaceByOptionsMenuState(sound_idx=self.sound_idx))
        elif action_name == self.OPTION_DELETE:
            self.remove_sound(self.spi.get_sound_property(self.sound_idx, PlStateNames.SOUND_UUID, '-'))
            self.sm.go_back()
        elif action_name == self.OPTION_OPEN_IN_FREESOUND:
            self.sm.show_global_message('Sound opened\nin browser')
            selected_sound_id = self.spi.get_sound_property(self.sound_idx, PlStateNames.SOUND_ID)
            if selected_sound_id != '-':
                self.sm.open_url_in_browser = 'https://freesound.org/s/{}'.format(selected_sound_id)
            self.sm.go_back()
        elif action_name == self.OPTION_PRECISION_EDITOR:
            self.sm.move_to(SoundSliceEditorState(sound_idx=self.sound_idx))
        elif action_name == self.OPTION_ASSIGNED_NOTES:
            self.sm.move_to(SoundAssignedNotesEditorState(sound_idx=self.sound_idx))
        elif action_name == self.OPTION_MIDI_CC:
            self.sm.move_to(
                MenuCallbackState(
                    items=self.get_midi_cc_items_for_midi_cc_assignments_menu(),
                    selected_item=0,
                    title1="MIDI CC mappings",
                    callback=self.handle_select_midi_cc_assignment,
                    shift_callback=self.handle_delete_midi_cc_assignment,
                    update_items_callback=self.get_midi_cc_items_for_midi_cc_assignments_menu,
                    help_pages=[['Sh+Enc remove current']],
                    go_back_n_times=0)
                # Don't go back because we go into another menu and we handle this individually in the callbacks of
                # the inside options
            )
        elif action_name == self.OPTION_GO_TO_SOUND:
            self.sm.move_to(EnterNoteState(
                title1="Go to sound...",
                callback=lambda note: (
                self.sm.go_back(n_times=3), self.sm.move_to(SoundSelectedState(self.get_sound_idx_from_note(note)))),
            ))
        elif action_name == self.OPTION_BOOKMARK:
            selected_sound_id = self.spi.get_sound_property(self.sound_idx, PlStateNames.SOUND_ID)
            if selected_sound_id != '-':
                result = bookmark_sound(selected_sound_id)
                if result:
                    self.sm.show_global_message('Freesound\nbookmark added!')
                else:
                    self.sm.show_global_message('Error bookmarking\n sound')
            else:
                self.sm.show_global_message('Can\'t bookmark non\nFreesound sound')
            self.sm.go_back()
        else:
            self.sm.show_global_message('Not implemented...')

    def get_midi_cc_items_for_midi_cc_assignments_menu(self):
        return [label for label in sorted(
            self.spi.get_sound_property(self.sound_idx, PlStateNames.SOUND_MIDI_CC_ASSIGNMENTS, default={}).keys())] + [
                   self.MIDI_CC_ADD_NEW_TEXT]

    def handle_select_midi_cc_assignment(self, midi_cc_assignment_label):
        if midi_cc_assignment_label == self.MIDI_CC_ADD_NEW_TEXT:
            # Show "new assignment" menu
            self.sm.move_to(EditMIDICCAssignmentState(sound_idx=self.sound_idx))
        else:
            midi_cc_assignment = self.spi.get_sound_property(self.sound_idx, PlStateNames.SOUND_MIDI_CC_ASSIGNMENTS,
                                                        default={}).get(midi_cc_assignment_label, None)
            if midi_cc_assignment is not None:
                # Show "edit assignment" menu
                self.sm.move_to(EditMIDICCAssignmentState(
                    cc_number=midi_cc_assignment[PlStateNames.SOUND_MIDI_CC_ASSIGNMENT_CC_NUMBER],
                    parameter_name=midi_cc_assignment[PlStateNames.SOUND_MIDI_CC_ASSIGNMENT_PARAM_NAME],
                    min_range=midi_cc_assignment[PlStateNames.SOUND_MIDI_CC_ASSIGNMENT_MIN_RANGE],
                    max_range=midi_cc_assignment[PlStateNames.SOUND_MIDI_CC_ASSIGNMENT_MAX_RANGE],
                    uuid=midi_cc_assignment[PlStateNames.SOUND_MIDI_CC_ASSIGNMENT_UUID],
                    sound_idx=self.sound_idx
                ))

    def handle_delete_midi_cc_assignment(self, midi_cc_assignment_label):
        midi_cc_assignment = self.spi.get_sound_property(self.sound_idx, PlStateNames.SOUND_MIDI_CC_ASSIGNMENTS,
                                                    default={}).get(midi_cc_assignment_label, None)
        if midi_cc_assignment is not None:
            self.spi.send_msg_to_plugin('/remove_cc_mapping',
                                   [self.spi.get_sound_property(self.sound_idx, PlStateNames.SOUND_UUID, '-'),
                                    midi_cc_assignment[PlStateNames.SOUND_MIDI_CC_ASSIGNMENT_UUID]])
            self.sm.show_global_message('Removing MIDI\nmapping...')