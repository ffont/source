from state_synchronizer import SourceStateSynchronizer
from helpers import StateNames, state_names_source_state_hierarchy_map


class SourcePluginInterface(object):
    sss = None
    extra_source_state = {}
    
    def __init__(self):
        self.sss = SourceStateSynchronizer(verbose=False)

    def send_osc_to_plugin(self, address, values):
        if self.sss.osc_client is not None:
            self.sss.osc_client.send_message(address, values)

    def update_source_extra_state(self, extra_state):
        self.extra_source_state.update(extra_state)

    def has_state(self):
        return self.sss.state_soup is not None

    def get_source_sound_idx_from_source_sampler_sound_uuid(self, source_sampler_sound_uuid):
        preset_state = self.sss.state_soup.find_all("PRESET".lower())[0]
        for sound_idx, sound in enumerate(preset_state.find_all("SOUND".lower())):
            if sound.get(StateNames.SOURCE_SAMPLER_SOUND_UUID.value.split('_')[0], "") == source_sampler_sound_uuid:
                return sound_idx
        return -1
   
    def get_property(self, property_name, default=None, sound_idx=None):
        
        def return_with_type(value):
            # if value is integer or float, return with correct type, otherwise return as is (as string)
            if value is not None and value.replace('.', '').isdigit():
                if '.' in value:
                    return float(value)
                else:
                    return int(value)
            return value
        
        if not self.has_state():
            return default
        
        # by default "sound" is returned as some sounds properties are not in state_names_source_state_hierarchy_map
        hierarchy_location = state_names_source_state_hierarchy_map.get(property_name, 'sound')

        if hasattr(property_name, 'value'):
            property_name = property_name.value  # If defined from StateNames enum, get the actual value
        
        if type(property_name) not in [int, StateNames] and 'name_' in property_name:
            property_name = StateNames.NAME.value

        if type(property_name) not in [int, StateNames] and 'uuid_' in property_name:
            property_name = StateNames.UUID.value

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
            if property_name == StateNames.NUM_SOUNDS.value:
                return len(sounds_state)
            elif property_name == StateNames.NUM_SOUNDS_CHANGED.value:
                # TODO: implement this properly
                # current_state.get(StateNames.NUM_SOUNDS, len(sounds_info)) != len(sounds_info)
                return False
            elif property_name == StateNames.NUM_SOUNDS_DOWNLOADING.value:
                return len([sound for sound in sounds_state if float(sound.find_all("SOUND_SAMPLE".lower())[0].get(StateNames.SOUND_DOWNLOAD_PROGRESS.value.lower(), 100)) < 100])
            elif property_name == StateNames.NUM_SOUNDS_LOADED_IN_SAMPLER.value:
                return len([sound for sound in sounds_state if sound.get(StateNames.SOUND_LOADED_IN_SAMPLER.value.lower(), '1') == '1'])
            elif property_name == StateNames.SOUND_SLICES:
                # TODO: implemrnt this
                '''
                slices = []
                try:
                    analysis_onsets = sound_info.find_all("ANALYSIS".lower())[0].find_all("onsets".lower())[0]
                    for onset in analysis_onsets.find_all('onset'):
                        slices.append(float(onset['onsettime']))
                except IndexError:
                    pass
                '''
                return []

            elif property_name == StateNames.REVERB_SETTINGS.value:
                return [
                    float(preset_state.get('reverbRoomSize'.lower(), 0.0)),
                    float(preset_state.get('reverbDamping'.lower(), 0.0)),
                    float(preset_state.get('reverbWetLevel'.lower(), 0.0)),
                    float(preset_state.get('reverbDryLevel'.lower(), 0.0)),
                    float(preset_state.get('reverbWidth'.lower(), 0.0)),
                    float(preset_state.get('reverbFreezeMode'.lower(), 0.0)),
                ]
 
        elif hierarchy_location == 'volatile':
            return self.sss.volatile_state.get(property_name, default)

    def get_sound_property(self, sound_idx, property_name, default=None):
        return self.get_property(property_name, sound_idx=sound_idx, default=default)
        
    def get_num_loaded_sounds(self):
        if not self.has_state():
            return 0
        return len(self.sss.state_soup.find_all("SOUND".lower()))
