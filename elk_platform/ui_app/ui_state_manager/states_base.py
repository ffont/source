import random
import time
import traceback

from freesound_interface import find_sound_by_similarity, find_sound_by_query, find_sounds_by_query, \
    find_random_sounds, find_sound_by_id
from helpers import justify_text, frame_from_lines, PlStateNames, add_scroll_bar_to_frame, \
    add_centered_value_to_frame, add_text_input_to_frame, \
    raw_assigned_notes_to_midi_assigned_notes, add_recent_query, get_recent_queries

from ui_state_manager.constants import predefined_queries, note_layout_types, ac_descriptors_names


class State(object):

    spi = None
    sm = None
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
            "M" if self.spi.get_property(PlStateNames.MIDI_RECEIVED, False) else "",
            "!" if not self.spi.get_property(PlStateNames.NETWORK_IS_CONNECTED) else "",
            "Q" if self.spi.get_property(PlStateNames.IS_QUERYING, False) else "",
            ["|", "/", "-", "\\"][self.sm.frame_counter % 4]
        )
        return {
            "invert": True, 
            "text": justify_text("{0}:{1}".format(
                self.spi.get_property(PlStateNames.LOADED_PRESET_INDEX, -1),
                self.spi.get_property(PlStateNames.LOADED_PRESET_NAME, 'NoName')),
                indicators)
        }

    def get_sound_idx_from_buttons(self, button_idx, shift=False):
        sound_idx = -1
        if button_idx > 0:
            sound_idx = button_idx - 1   # from 0-7
            if shift:  # if "shift" button is pressed, sound index is form 8-15
                sound_idx += 8
    
            num_sounds = self.spi.get_property(PlStateNames.NUM_SOUNDS, 0)
            if sound_idx < num_sounds:
                return sound_idx
            else:
                return num_sounds
        return sound_idx

    def save_current_preset(self):
        current_preset_name = self.spi.get_property(PlStateNames.LOADED_PRESET_NAME, 'NoName')
        current_preset_index = self.spi.get_property(PlStateNames.LOADED_PRESET_INDEX, -1)
        if current_preset_index > -1:
            self.spi.send_msg_to_plugin("/save_preset", [current_preset_name, int(current_preset_index)])
            self.sm.show_global_message("Saving {}\n{}...".format(current_preset_index, current_preset_name))

    def save_current_preset_to(self, query="", preset_idx=-1):
        preset_name = query  # NOTE: the parameter is called "query" because it reuses some classes used for entering queries. We might want to change that to "name"
        self.spi.send_msg_to_plugin("/save_preset", [preset_name, int(preset_idx)])
        self.sm.show_global_message("Saving {}\n{}...".format(preset_idx, preset_name))

    def load_preset(self, preset_idx):
        preset_name = ""
        if ':' in preset_idx:
            preset_name = preset_idx.split(':')[1]
            preset_idx = int(preset_idx.split(':')[0])  # This is because we're passing preset_idx:preset_name format (some callbacks might use it)
        self.spi.send_msg_to_plugin("/load_preset", [int(preset_idx)])
        if preset_name:
            self.sm.show_global_message("Loading {}\n{}...".format(preset_idx, preset_name), duration=2)
        else:
            self.sm.show_global_message("Loading {}...".format(preset_idx), duration=2)

    def reload_current_preset(self):
        current_preset_index = self.spi.get_property(PlStateNames.LOADED_PRESET_INDEX, -1)
        current_preset_name = self.spi.get_property(PlStateNames.LOADED_PRESET_NAME, "unnamed")
        if current_preset_index > -1:
            self.spi.send_msg_to_plugin("/load_preset", [int(current_preset_index)])
            self.sm.show_global_message("Loading {}\n{}...".format(current_preset_index, current_preset_name), duration=2)

    def reapply_note_layout(self, layout_type):
        self.spi.send_msg_to_plugin("/reapply_layout", [['Contiguous', 'Interleaved'].index(layout_type)])
        self.sm.show_global_message("Updated layout")

    def set_num_voices(self, num_voices):
        self.spi.send_msg_to_plugin("/set_polyphony", [num_voices])

    def set_midi_in_chhannel(self, midi_channel):
        self.spi.send_msg_to_plugin("/set_midi_in_channel", [midi_channel])

    def set_download_original_files(self, preference):
        self.spi.send_msg_to_plugin("/set_use_original_files", [{
            'Never': 'never',
            'Only small': 'onlyShort',
            'Always': 'always',
        }[preference]])
        self.spi.send_oauth_token_to_plugin()

    def send_add_or_replace_sound_to_plugin(self, sound_uuid, new_sound, assigned_notes="", root_note=-1, local_file_path="", move_once_loaded=False):
        sound_onsets_list = []
        if 'analysis' in new_sound and new_sound['analysis'] is not None:
            if 'rhythm' in new_sound['analysis']:
                if 'onset_times' in new_sound['analysis']['rhythm']:
                    sound_onsets_list = new_sound['analysis']['rhythm']['onset_times']
                    if type(sound_onsets_list) != list:
                        # This can happen because when there's only one onset, FS api returns it as a number instead of a list of one element
                        sound_onsets_list = [sound_onsets_list]
        # Don't send more than 128 onsets because these won't be mapped anyway
        # Sending too many onsets might render the message too long
        sound_onsets_list = sound_onsets_list[:128]  

        common_arguments = [ 
            new_sound['id'], 
            new_sound['name'], 
            new_sound['username'], 
            new_sound['license'], 
            new_sound['previews']['preview-hq-ogg'],
            local_file_path,
            new_sound['type'],
            new_sound['filesize'],
            ','.join(str(o) for o in sound_onsets_list),  # for use as slices
            assigned_notes,  # This should be string representation of hex number
            root_note, # The note to be used as root
        ]

        if sound_uuid == "":
            # Adding a new sound
            self.spi.send_msg_to_plugin("/add_sound", common_arguments)
        else:
            # Replacing existing sound
            self.spi.send_msg_to_plugin("/replace_sound", [sound_uuid] + common_arguments )

        if sound_uuid == "" and move_once_loaded:
            self.sm.set_waiting_to_go_to_last_loaded_sound()

    def load_query_results(self, new_sounds, note_mapping_type=1, assigned_notes_per_sound_list=[]):
        self.spi.send_msg_to_plugin('/clear_all_sounds', [])
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

            self.send_add_or_replace_sound_to_plugin("", sound, assigned_notes=midi_notes, root_note=root_note)

    def consolidate_ac_descriptors_from_kwargs(self, kwargs_dict):
        return {key: value for key, value in kwargs_dict.items() if key in ac_descriptors_names and value != 'off'}


    def add_or_replace_sound_by_query(self, sound_uuid='', query='', min_length=0, max_length=300, page_size=50, move_once_loaded=False, **kwargs):
        self.sm.show_global_message("Searching\n{}...".format(query), duration=3600)
        self.sm.block_ui_input = True
        try:
            selected_sound = find_sound_by_query(query=query, min_length=min_length, max_length=max_length, page_size=page_size, license=kwargs.get('license', None), ac_descriptors_filters=self.consolidate_ac_descriptors_from_kwargs(kwargs))
            if selected_sound is not None:
                self.sm.show_global_message("Loading sound...")
                self.send_add_or_replace_sound_to_plugin(sound_uuid, selected_sound, move_once_loaded=move_once_loaded)
            else:
                self.sm.show_global_message("No results found!")
        except Exception as e:
            print("ERROR while querying Freesound: {0}".format(e))
            traceback.print_tb(e.__traceback__)
            self.sm.show_global_message("Error :(")
        self.sm.block_ui_input = False

    def replace_sound_by_similarity(self, sound_uuid, selected_sound_id):
        self.sm.block_ui_input = True
        self.sm.show_global_message("Searching\nsimilar...", duration=3600)
        try:
            selected_sound = find_sound_by_similarity(selected_sound_id)
            if selected_sound is not None:
                self.sm.show_global_message("Replacing sound...")
                self.send_add_or_replace_sound_to_plugin(sound_uuid, selected_sound)
            else:
                self.sm.show_global_message("No results found!")
        except Exception as e:
            print("ERROR while querying Freesound: {0}".format(e))
            traceback.print_tb(e.__traceback__)
            self.sm.show_global_message("Error :(")
        self.sm.block_ui_input = False

    def add_or_replace_sound_random(self, sound_uuid='', num_sounds=1, min_length=0, max_length=10, layout=1, move_once_loaded=False, **kwargs):
        self.sm.show_global_message("Finding\nrandom...", duration=3600)
        self.sm.block_ui_input = True
        try:
            new_sound = find_random_sounds(n_sounds=1, min_length=min_length, max_length=max_length, license=kwargs.get('license', None), ac_descriptors_filters=self.consolidate_ac_descriptors_from_kwargs(kwargs))
            if not new_sound:
                self.sm.show_global_message("No sound found!")
            else:
                new_sound = new_sound[0]
                self.sm.show_global_message("Loading sound...")
                self.send_add_or_replace_sound_to_plugin(sound_uuid, new_sound, move_once_loaded=move_once_loaded)
        except Exception as e:
            print("ERROR while querying Freesound: {0}".format(e))
            traceback.print_tb(e.__traceback__)
            self.sm.show_global_message("Error :(")
        self.sm.block_ui_input = False

    def new_preset_by_query(self, query='', num_sounds=8, min_length=0, max_length=300, layout=1, page_size=150, **kwargs):
        self.sm.show_global_message("Searching\n{}...".format(query), duration=3600)
        self.sm.block_ui_input = True
        try:
            new_sounds = find_sounds_by_query(query=query, n_sounds=num_sounds, min_length=min_length, max_length=max_length, page_size=page_size, license=kwargs.get('license', None), ac_descriptors_filters=self.consolidate_ac_descriptors_from_kwargs(kwargs))
            if not new_sounds:
                self.sm.show_global_message("No results found!")
            else:
                self.sm.show_global_message("Loading sounds...")
                self.load_query_results(new_sounds, layout)
                
        except Exception as e:
            print("ERROR while querying Freesound: {0}".format(e))
            traceback.print_tb(e.__traceback__)
            self.sm.show_global_message("Error :(")
        self.sm.block_ui_input = False

    def new_preset_from_predefined_query(self, query, **kwargs):
        kwargs.update({"query": query})
        self.new_preset_by_query(**kwargs)  

    def new_preset_by_random_sounds(self, num_sounds=16, min_length=0, max_length=10, layout=1, **kwargs):

        def show_progress_message(count, total):
            self.sm.show_global_message("Entering the\nunknown... [{}/{}]".format(count + 1, total), duration=3600)
        
        self.sm.show_global_message("Entering the\nunknown...", duration=3600)
        self.sm.block_ui_input = True
        try:
            new_sounds = find_random_sounds(n_sounds=num_sounds, min_length=min_length, max_length=max_length, report_callback=show_progress_message, license=kwargs.get('license', None), ac_descriptors_filters=self.consolidate_ac_descriptors_from_kwargs(kwargs))
            if not new_sounds:
                self.sm.show_global_message("No results found!")
            else:
                self.sm.show_global_message("Loading sounds...")
                self.load_query_results(new_sounds, layout)

        except Exception as e:
            print("ERROR while querying Freesound: {0}".format(e))
            traceback.print_tb(e.__traceback__)
            self.sm.show_global_message("Error :(")
        self.sm.block_ui_input = False

    def new_preset_by_similar_sounds(self, note_mapping_type=1):

        def show_progress_message(count, total):
            self.sm.show_global_message("Searching similar\nsounds... [{}/{}]".format(count + 1, total), duration=3600)

        self.sm.show_global_message("Searching similar\nsounds...", duration=3600)
        self.sm.block_ui_input = True
        try:
            new_sounds = []
            existing_sounds_assigned_notes = []
            for sound_idx in range(0, self.spi.get_num_loaded_sounds()):
                existing_sounds_assigned_notes.append(self.spi.get_sound_property(self.sound_idx, PlStateNames.SOUND_ASSIGNED_NOTES))
                sound_id = self.spi.get_sound_property(sound_idx, PlStateNames.SOUND_ID)
                if sound_id != 0:
                    new_sound = find_sound_by_similarity(sound_id)
                    if new_sound is not None:
                        new_sounds.append(new_sound)
                    else:
                        # If not similar sound is found, try searching for the same sound that was there
                        same_sound = find_sound_by_id(sound_id)
                        if same_sound is not None:
                            new_sounds.append(same_sound)
                    show_progress_message(sound_idx + 1, self.spi.get_num_loaded_sounds())

            self.sm.show_global_message("Loading sounds...")
            self.load_query_results(new_sounds, note_mapping_type=note_mapping_type, assigned_notes_per_sound_list=existing_sounds_assigned_notes)

        except Exception as e:
            print("ERROR while querying Freesound: {0}".format(e))
            traceback.print_tb(e.__traceback__)
            self.sm.show_global_message("Error :(")
        self.sm.block_ui_input = False

    def set_slices_for_sound(self, sound_uuid, slices):
        if sound_uuid != "":
            self.spi.send_msg_to_plugin('/set_slices', [sound_uuid, ','.join([str(s) for s in slices])])

    def get_sound_idx_from_note(self, midi_note):
        # Iterate over sounds to find the first one that has that note assigned
        for sound_idx in range(0, self.spi.get_num_loaded_sounds()):
            raw_assigned_notes = self.spi.get_sound_property(sound_idx, PlStateNames.SOUND_ASSIGNED_NOTES)
            if raw_assigned_notes is not None:
                if midi_note in raw_assigned_notes_to_midi_assigned_notes(raw_assigned_notes):
                    return sound_idx
        return -1

    def set_assigned_notes_for_sound(self, sound_uuid, assigned_notes, root_note=None):
        # assigned_notes here is a list with the midi note numbers
        assigned_notes = sorted(list(set(assigned_notes)))  # make sure they're all sorted and there are no duplicates
        if root_note is None:
            if assigned_notes:
                root_note = assigned_notes[len(assigned_notes)//2]  # Default to the middle note of the assigned notes
            else:
                root_note = 69
        if sound_uuid != "":
            assigned_notes_aux = ['0'] * 128
            for note in assigned_notes:
                assigned_notes_aux[note] = '1'
            midi_notes = hex(int("".join(reversed(''.join(assigned_notes_aux))), 2))  # Convert to a format expected by send_add_or_replace_sound_to_plugin
            self.spi.send_msg_to_plugin('/set_assigned_notes', [sound_uuid, midi_notes, root_note])

    def remove_sound(self, sound_uuid):
        self.spi.send_msg_to_plugin('/remove_sound', [sound_uuid])
        self.sm.show_global_message("Sound removed!")

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
            self.sm.go_back()
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
            current_preset_index = self.spi.get_property(PlStateNames.LOADED_PRESET_INDEX, -1)
            if current_preset_index > -1:
                current_preset_index += direction
                if current_preset_index < 0:
                    current_preset_index = 0
                self.spi.send_msg_to_plugin("/load_preset", [current_preset_index])
        else:
            super().on_encoder_rotated(direction, shift=True)


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
            self.selected_item = 0

    def previous_item(self):
        self.selected_item -= 1
        if self.selected_item < 0:
            self.selected_item = self.num_items -1

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
                self.sm.go_back(n_times=self.go_back_n_times)
            else:
                if shift:
                    self.shift_callback(self.selected_item_name)
                    self.sm.go_back(n_times=self.go_back_n_times_shift_callback)
                else:
                    if self.extra_data_for_callback is None:
                        self.callback(self.selected_item_name)
                    else:
                        self.callback(self.selected_item_name, **self.extra_data_for_callback)
                    self.sm.go_back(n_times=self.go_back_n_times)

    def on_source_state_update(self):
        if self.update_items_callback is not None:
            self.items = self.update_items_callback()
            if self.selected_item >= len(self.items):
                self.selected_item = len(self.items) - 1


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
        self.sm.go_back(n_times=self.go_back_n_times)


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

        if self.spi.get_property(PlStateNames.MIDI_RECEIVED, False):
            last_note_received = self.spi.get_property(PlStateNames.LAST_NOTE_MIDI_RECEIVED, -1)
        else:
            last_note_received = -1

        if last_note_received != -1:
            self.sm.go_back(n_times=self.go_back_n_times)
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
        self.sm.go_back(n_times=self.go_back_n_times)


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
        self.sm.go_back(n_times=self.go_back_n_times)
        

class EnterTextViaHWOrWebInterfaceState(ShowHelpPagesMixin, EnterDataViaWebInterfaceState):

    max_length = 100
    current_text = []
    cursor_position = 0
    available_chars = [char for char in " abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789:"]
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

