import os

from collections import defaultdict

from .state_synchronizer import SourceStateSynchronizer
from .helpers import PlStateNames, state_names_source_state_hierarchy_map, sound_parameters_info_dict, \
    translate_cc_license_url


class SourcePluginInterface(object):
    sss = None
    extra_source_state = {}
    ui_state_manager = None

    def __init__(self, ui_state_manager):
        self.sss = SourceStateSynchronizer(ui_state_manager, verbose=True)

    def send_msg_to_plugin(self, address, values):
        self.sss.send_msg_to_plugin(address, values)

    def update_source_extra_state(self, extra_state):
        self.extra_source_state.update(extra_state)

    def has_state(self):
        return self.sss.state_soup is not None

    def check_if_fs_oauth_token_should_be_set(self):
        token = self.get_property(PlStateNames.FREESOUND_OAUTH_TOKEN, '')
        if not token:
            # If no token in the plugin state, get the token from the UI state and send it
            self.send_oauth_token_to_plugin()

    def send_oauth_token_to_plugin(self):
        if self.ui_state_manager is not None:
            stored_token = self.ui_state_manager.get_stored_access_token()
            if stored_token:
                self.send_msg_to_plugin("/set_oauth_token", [stored_token])

    def get_source_sound_idx_from_source_sampler_sound_uuid(self, source_sampler_sound_uuid):
        for sound_idx, sound in enumerate(self.sss.state_soup.find_all("SOUND".lower())):
            if self.get_sound_property(sound_idx, PlStateNames.SOURCE_SAMPLER_SOUND_UUID,
                                       "") == source_sampler_sound_uuid:
                return sound_idx
        return -1

    def get_property(self, property_name, default=None, sound_idx=None):

        def return_with_type(value, property_name=None):
            # if value is integer or float, return with correct type, otherwise return as is (as string)
            # also if value is in exceptions list, return it without transformation
            if property_name.lower() in [PlStateNames.SOUND_ASSIGNED_NOTES.lower(), PlStateNames.NAME.lower()]:
                return value

            if value is None or type(value) != str:
                return value

            if value.replace('.', '').isdigit():
                if '.' in value:
                    return float(value)
                else:
                    return int(value)
            return value

        if not self.has_state():
            return default

        # by default "sound" is returned as some sounds properties are not in state_names_source_state_hierarchy_map
        hierarchy_location = state_names_source_state_hierarchy_map.get(property_name, 'sound')

        if 'name_' in property_name:
            property_name = PlStateNames.NAME

        if 'uuid_' in property_name:
            property_name = PlStateNames.UUID

        source_state = self.sss.state_soup
        try:
            preset_state = source_state.find_all("PRESET".lower())[0]
        except IndexError:
            # If no PRESET in state, then state is not available, return default
            return default
        sounds_state = preset_state.find_all("SOUND".lower())

        if sound_idx is not None and sound_idx >= len(sounds_state):
            return default

        sound_state = None
        sound_sample_state = None
        if sound_idx is not None:
            try:
                sound_state = sounds_state[sound_idx]
                sound_sample_state = sound_state.find_all("SOUND_SAMPLE".lower())[0]
            except IndexError:
                return default

        if hierarchy_location == 'extra_state':
            return self.extra_source_state.get(property_name, default)

        elif hierarchy_location == 'source_state':
            return return_with_type(source_state.get(property_name.lower(), default), property_name)

        elif hierarchy_location == 'preset':
            return return_with_type(preset_state.get(property_name.lower(), default), property_name)

        elif hierarchy_location == 'sound':
            return return_with_type(sound_state.get(property_name.lower(), default), property_name)

        elif hierarchy_location == 'sound_sample':
            if property_name == PlStateNames.SOUND_DOWNLOAD_PROGRESS:
                # Special case of download progress, calculate the average over all existing sound sample sounds
                download_percentage = 0
                for ss in sound_state.find_all("SOUND_SAMPLE".lower()):
                    download_percentage += float(ss.get(PlStateNames.SOUND_DOWNLOAD_PROGRESS.lower(), 0.0))
                download_percentage = download_percentage / len(sound_state.find_all("SOUND_SAMPLE".lower()))
                return download_percentage
            elif property_name == PlStateNames.SOUND_DURATION:
                total_duration = 0
                for ss in sound_state.find_all("SOUND_SAMPLE".lower()):
                    total_duration += float(ss.get(PlStateNames.SOUND_DURATION.lower(), 0.0))
                return total_duration
            elif property_name == PlStateNames.SOUND_FILESIZE:
                total_size = 0
                for ss in sound_state.find_all("SOUND_SAMPLE".lower()):
                    total_size += float(ss.get(PlStateNames.SOUND_FILESIZE.lower(), 0.0))
                return total_size
            elif property_name == PlStateNames.SOUND_LICENSE:
                licenses = []
                for ss in sound_state.find_all("SOUND_SAMPLE".lower()):
                    licenses.append(ss.get(PlStateNames.SOUND_LICENSE.lower(), None))
                licenses = list(set([translate_cc_license_url(l) for l in licenses if l is not None]))
                if len(licenses) > 1:
                    return 'multiple'
                elif len(licenses) == 1:
                    return licenses[0]
                else:
                    return '-'
            elif property_name == PlStateNames.SOUND_AUTHOR:
                authors = []
                for ss in sound_state.find_all("SOUND_SAMPLE".lower()):
                    authors.append(ss.get(PlStateNames.SOUND_AUTHOR.lower(), None))
                authors = list(set([a for a in authors if a is not None]))
                if len(authors) > 1:
                    return 'multiple'
                elif len(authors) == 1:
                    return authors[0]
                else:
                    return '-'
            elif property_name == PlStateNames.SOUND_TYPE:
                types = []
                for ss in sound_state.find_all("SOUND_SAMPLE".lower()):
                    types.append(ss.get(PlStateNames.SOUND_TYPE.lower(), None))
                types = list(set([t for t in types if t is not None]))
                if len(types) > 1:
                    return 'multiple'
                elif len(types) == 1:
                    return types[0]
                else:
                    return '-'
            elif property_name == PlStateNames.SOUND_ID:
                ids = []
                for ss in sound_state.find_all("SOUND_SAMPLE".lower()):
                    ids.append(ss.get(PlStateNames.SOUND_ID.lower(), None))
                ids = list(set([t for t in ids if t is not None]))
                if len(ids) > 1:
                    return 'multiple'
                elif len(ids) == 1:
                    return int(ids[0])
                else:
                    return -1
            return return_with_type(sound_sample_state.get(property_name.lower(), default), property_name)

        elif hierarchy_location == 'computed':
            if property_name == PlStateNames.NUM_SOUNDS:
                return len(sounds_state)

            elif property_name == PlStateNames.NUM_SOUNDS_DOWNLOADING:
                return len([sound for sound in sounds_state if float(
                    sound.find_all("SOUND_SAMPLE".lower())[0].get(PlStateNames.SOUND_DOWNLOAD_PROGRESS.lower(),
                                                                  100)) < 100])

            elif property_name == PlStateNames.NUM_SOUNDS_LOADED_IN_SAMPLER:
                return len([sound for sound in sounds_state if
                            sound.get(PlStateNames.SOUND_LOADED_IN_SAMPLER.lower(), '1') == '1'])

            elif property_name == PlStateNames.SOUND_SLICES:
                slices = []
                try:
                    analysis_onsets = sound_sample_state.find_all("ANALYSIS".lower())[0].find_all("onsets".lower())[0]
                    for onset in analysis_onsets.find_all('onset'):
                        slices.append(float(onset['onsettime']))
                except IndexError:
                    pass
                return slices

            elif property_name == PlStateNames.REVERB_SETTINGS:
                return [
                    float(preset_state.get('reverbRoomSize'.lower(), 0.0)),
                    float(preset_state.get('reverbDamping'.lower(), 0.0)),
                    float(preset_state.get('reverbWetLevel'.lower(), 0.0)),
                    float(preset_state.get('reverbDryLevel'.lower(), 0.0)),
                    float(preset_state.get('reverbWidth'.lower(), 0.0)),
                    float(preset_state.get('reverbFreezeMode'.lower(), 0.0)),
                ]

            elif property_name == PlStateNames.SOUND_MIDI_CC_ASSIGNMENTS:
                # Concoslidate MIDI CC mapping data into a dict
                # NOTE: We use some complex logic here in order to allow several mappings with the same CC#/Parameter name
                processed_sound_midi_cc_info = {}
                processed_sound_midi_cc_info_list_aux = []
                # TODO: update midi mappings to MIDI_CC_MAPPING
                for midi_cc in sound_state.find_all('midi_cc_mapping'.lower()):
                    cc_number = int(midi_cc['ccNumber'.lower()])
                    parameter_name = midi_cc['parameterName'.lower()]
                    label = 'CC#{}->{}'.format(cc_number, sound_parameters_info_dict[parameter_name][
                        2])  # Be careful, if sound_parameters_info_dict structure changes, this won't work
                    processed_sound_midi_cc_info_list_aux.append((label, {
                        PlStateNames.SOUND_MIDI_CC_ASSIGNMENT_PARAM_NAME: parameter_name,
                        PlStateNames.SOUND_MIDI_CC_ASSIGNMENT_CC_NUMBER: cc_number,
                        PlStateNames.SOUND_MIDI_CC_ASSIGNMENT_MIN_RANGE: float(midi_cc['minRange'.lower()]),
                        PlStateNames.SOUND_MIDI_CC_ASSIGNMENT_MAX_RANGE: float(midi_cc['maxRange'.lower()]),
                        PlStateNames.SOUND_MIDI_CC_ASSIGNMENT_UUID: midi_cc['uuid'.lower()],
                    }))
                # Sort by assignment random ID and then for label. In this way the sorting will be always consistent
                processed_sound_midi_cc_info_list_aux = sorted(processed_sound_midi_cc_info_list_aux,
                                                               key=lambda x: x[1][
                                                                   PlStateNames.SOUND_MIDI_CC_ASSIGNMENT_UUID])
                processed_sound_midi_cc_info_list_aux = sorted(processed_sound_midi_cc_info_list_aux,
                                                               key=lambda x: x[0])
                assignment_labels = [label for label, _ in processed_sound_midi_cc_info_list_aux]
                added_labels_count = defaultdict(int)
                for assignment_label, assignment_data in processed_sound_midi_cc_info_list_aux:
                    added_labels_count[assignment_label] += 1
                    if added_labels_count[assignment_label] > 1:
                        assignment_label = '{0} ({1})'.format(assignment_label, added_labels_count[assignment_label])
                    processed_sound_midi_cc_info[assignment_label] = assignment_data

                return processed_sound_midi_cc_info

        elif hierarchy_location == 'volatile':
            return self.sss.volatile_state.get(property_name, default)

    def get_sound_property(self, sound_idx, property_name, default=None):
        if self.sss.state_soup is None:
            return default
        return self.get_property(property_name, sound_idx=sound_idx, default=default)

    def get_num_loaded_sounds(self):
        if not self.has_state():
            return 0
        return len(self.sss.state_soup.find_all("SOUND".lower()))

    def get_source_sampler_sounds_per_sound(self, sound_idx):
        return self.sss.state_soup.find_all("SOUND".lower())[sound_idx].find_all("SOUND_SAMPLE".lower())

    def get_num_source_sampler_sounds_per_sound(self, sound_idx):
        return len(self.get_source_sampler_sounds_per_sound(sound_idx))

    def get_local_audio_files_path(self):
        if self.has_state():
            base_path = self.get_property(PlStateNames.SOURCE_DATA_LOCATION, None)
            if base_path is not None:
                base_path = os.path.join(base_path, 'local_files')
                if not os.path.exists(base_path):
                    os.makedirs(base_path)
                return base_path
        return None

    def get_sound_audio_files_path(self):
        if self.has_state():
            base_path = self.get_property(PlStateNames.SOUNDS_DATA_LOCATION, None)
            if base_path is not None:
                if not os.path.exists(base_path):
                    os.makedirs(base_path)
                return base_path
        return None

    def get_preset_files_path(self):
        if self.has_state():
            base_path = self.get_property(PlStateNames.PRESETS_DATA_LOCATION, None)
            if base_path is not None:
                if not os.path.exists(base_path):
                    os.makedirs(base_path)
                return base_path
        return None
