from collections import defaultdict

from state_synchronizer import SourceStateSynchronizer
from helpers import StateNames, state_names_source_state_hierarchy_map, sound_parameters_info_dict


class SourcePluginInterface(object):
    sss = None
    extra_source_state = {}
    ui_state_manager = None
    
    def __init__(self, ui_state_manager):
        self.sss = SourceStateSynchronizer(ui_state_manager, verbose=False)

    def send_osc_to_plugin(self, address, values):
        if self.sss.osc_client is not None:
            self.sss.osc_client.send_message(address, values)

    def update_source_extra_state(self, extra_state):
        self.extra_source_state.update(extra_state)

    def has_state(self):
        return self.sss.state_soup is not None

    def get_source_sound_idx_from_source_sampler_sound_uuid(self, source_sampler_sound_uuid):
        for sound_idx, sound in enumerate(self.sss.state_soup.find_all("SOUND".lower())):
            if self.get_sound_property(sound_idx, StateNames.SOURCE_SAMPLER_SOUND_UUID, "") == source_sampler_sound_uuid:
                return sound_idx
        return -1
   
    def get_property(self, property_name, default=None, sound_idx=None):
        
        def return_with_type(value):
            # if value is integer or float, return with correct type, otherwise return as is (as string)
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
            property_name = StateNames.NAME

        if 'uuid_' in property_name:
            property_name = StateNames.UUID

        source_state = self.sss.state_soup
        preset_state = source_state.find_all("PRESET".lower())[0]
        sounds_state = preset_state.find_all("SOUND".lower())

        if sound_idx is not None and sound_idx >= len(sounds_state):
            return default

        sound_state = None
        sound_sample_state = None
        if sound_idx is not None:
            sounds_state = preset_state.find_all("SOUND".lower())
            sound_state = sounds_state[sound_idx]
            sound_sample_state = sound_state.find_all("SOUND_SAMPLE".lower())[0]

        if hierarchy_location == 'extra_state':
            return self.extra_source_state.get(property_name, default)
        
        elif hierarchy_location == 'source_state':
            return return_with_type(source_state.get(property_name.lower(), default))
        
        elif hierarchy_location == 'preset':
            return return_with_type(preset_state.get(property_name.lower(), default))
       
        elif hierarchy_location == 'sound':
            return return_with_type(sound_state.get(property_name.lower(), default))
        
        elif hierarchy_location == 'sound_sample':
            return return_with_type(sound_sample_state.get(property_name.lower(), default))
        
        elif hierarchy_location == 'computed':
            if property_name == StateNames.NUM_SOUNDS:
                return len(sounds_state)
            
            elif property_name == StateNames.NUM_SOUNDS_DOWNLOADING:
                return len([sound for sound in sounds_state if float(sound.find_all("SOUND_SAMPLE".lower())[0].get(StateNames.SOUND_DOWNLOAD_PROGRESS.lower(), 100)) < 100])
            
            elif property_name == StateNames.NUM_SOUNDS_LOADED_IN_SAMPLER:
                return len([sound for sound in sounds_state if sound.get(StateNames.SOUND_LOADED_IN_SAMPLER.lower(), '1') == '1'])
            
            elif property_name == StateNames.SOUND_SLICES:
                slices = []
                try:
                    analysis_onsets = sound_sample_state.find_all("ANALYSIS".lower())[0].find_all("onsets".lower())[0]
                    for onset in analysis_onsets.find_all('onset'):
                        slices.append(float(onset['onsettime']))
                except IndexError:
                    pass
                return slices
            
            elif property_name == StateNames.REVERB_SETTINGS:
                return [
                    float(preset_state.get('reverbRoomSize'.lower(), 0.0)),
                    float(preset_state.get('reverbDamping'.lower(), 0.0)),
                    float(preset_state.get('reverbWetLevel'.lower(), 0.0)),
                    float(preset_state.get('reverbDryLevel'.lower(), 0.0)),
                    float(preset_state.get('reverbWidth'.lower(), 0.0)),
                    float(preset_state.get('reverbFreezeMode'.lower(), 0.0)),
                ]

            elif property_name == StateNames.SOUND_MIDI_CC_ASSIGNMENTS:
                # Concoslidate MIDI CC mapping data into a dict
                # NOTE: We use some complex logic here in order to allow several mappings with the same CC#/Parameter name
                processed_sound_midi_cc_info = {}
                processed_sound_midi_cc_info_list_aux = []
                # TODO: update midi mappings to MIDI_CC_MAPPING
                for midi_cc in sound_state.find_all('midi_cc_mapping'.lower()): 
                    cc_number = int(midi_cc['ccNumber'.lower()])
                    parameter_name = midi_cc['parameterName'.lower()]
                    label = 'CC#{}->{}'.format(cc_number, sound_parameters_info_dict[parameter_name][2])  # Be careful, if sound_parameters_info_dict structure changes, this won't work
                    processed_sound_midi_cc_info_list_aux.append((label, {
                        StateNames.SOUND_MIDI_CC_ASSIGNMENT_PARAM_NAME: parameter_name,
                        StateNames.SOUND_MIDI_CC_ASSIGNMENT_CC_NUMBER: cc_number,
                        StateNames.SOUND_MIDI_CC_ASSIGNMENT_MIN_RANGE: float(midi_cc['minRange'.lower()]),
                        StateNames.SOUND_MIDI_CC_ASSIGNMENT_MAX_RANGE: float(midi_cc['maxRange'.lower()]),
                        StateNames.SOUND_MIDI_CC_ASSIGNMENT_UUID: midi_cc['uuid'.lower()],
                    }))
                # Sort by assignment random ID and then for label. In this way the sorting will be always consistent
                processed_sound_midi_cc_info_list_aux = sorted(processed_sound_midi_cc_info_list_aux, key=lambda x:x[1][StateNames.SOUND_MIDI_CC_ASSIGNMENT_UUID])
                processed_sound_midi_cc_info_list_aux = sorted(processed_sound_midi_cc_info_list_aux, key=lambda x:x[0])
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
        return self.get_property(property_name, sound_idx=sound_idx, default=default)
        
    def get_num_loaded_sounds(self):
        if not self.has_state():
            return 0
        return len(self.sss.state_soup.find_all("SOUND".lower()))
