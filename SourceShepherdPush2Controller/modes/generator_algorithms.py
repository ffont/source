import random
import numpy as np

try:
    import os, sys
    # Add specific directory to python so we can import groove transformer from its folders
    sys.path.append(os.path.join(os.path.dirname(os.path.realpath(__file__)), 'GrooveTransformerExample/'))
    from .GrooveTransformerExample.helpers import load_mgt_model, predict_using_h_v_o, strip_note_from_hvo
    with_gt = True
except ImportError as e:
    print('Can\'t load pytorch, groove transformer algorithm will not work: {}'.format(e))
    with_gt = False

from utils import clamp


class GeneratorAlogorithm(object):
    parameters = []
    name = ''
    clip_edit_mode = None

    def __init__(self, clip_edit_mode):
        self.clip_edit_mode = clip_edit_mode  # Keep reference to ClipEditMode object so we can acces state details and other related stuff
        for key, param_data in self.parameters.items():
            param_data.update({'name': key, 'value': param_data['default']})

    def get_algorithm_parameters(self):
        return list(self.parameters.values())

    def generate_sequence(self):
        raise NotImplementedError

    def update_parameter_value(self, name, increment):
        # increment will be the increment sent form the encoder moving. small increments are -1/+1
        self.parameters[name]['value'] = clamp(
            self.parameters[name]['value'] + increment * self.parameters[name]['increment_scale'],
            self.parameters[name]['min'],
            self.parameters[name]['max'],
        )


class RandomGeneratorAlgorithm(GeneratorAlogorithm):
    name = 'Rnd+'
    parameters = {
        'length': {'display_name': 'LENGTH', 'type': float, 'min': 1.0, 'max': 32.0, 'default': 8.0,
                   'increment_scale': 1.0},
        'density': {'display_name': 'DENSITY', 'type': int, 'min': 1, 'max': 15, 'default': 5, 'increment_scale': 1},
        'max_duration': {'display_name': 'MAX DUR', 'type': float, 'min': 0.1, 'max': 10.0, 'default': 0.5,
                         'increment_scale': 0.125},
    }

    def generate_sequence(self):
        if self.parameters['length']['value'] > 0.0:
            new_clip_length = self.parameters['length']['value']
        else:
            new_clip_length = random.randint(5, 13)
        random_sequence = []
        for i in range(0, abs(self.parameters['density']['value'] + random.randint(-2, 2))):
            timestamp = (new_clip_length - 0.5) * random.random()
            duration = max(0.1, random.random() * self.parameters['max_duration']['value'])
            random_sequence.append(
                {'type': 1, 'midiNote': random.randint(64, 85), 'midiVelocity': 1.0, 'timestamp': timestamp,
                 'duration': duration}
            )
        return random_sequence, new_clip_length


class GrooveTransfomer(GeneratorAlogorithm):
    name = 'GTrans'
    parameters = {
        'threshold': {'display_name': 'DENSITY', 'type': float, 'min': 0.0, 'max': 1.0, 'default': 0.5,
                   'increment_scale': 0.1},
    }

    gt_model = None

    drum_mappings = {
        "KICK": [36],
        "SNARE": [38],
        "HH_CLOSED": [42],
        "HH_OPEN": [46],
        "TOM_3_LO": [43],
        "TOM_2_MID": [47],
        "TOM_1_HI": [50],
        "CRASH": [49],
        "RIDE":  [51]
    }

    sampling_thresholds = {
        "KICK": 0.5,
        "SNARE": 0.5,
        "HH_CLOSED": 0.5,
        "HH_OPEN": 0.5,
        "TOM_3_LO": 0.5,
        "TOM_2_MID": 0.5,
        "TOM_1_HI": 0.5,
        "CRASH": 0.5,
        "RIDE":  0.5
    }

    max_counts_allowed = {
        "KICK": 16,
        "SNARE": 8,
        "HH_CLOSED": 32,
        "HH_OPEN": 32,
        "TOM_3_LO": 32,
        "TOM_2_MID": 32,
        "TOM_1_HI": 32,
        "CRASH": 32,
        "RIDE":  32,
    }

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        if with_gt:
            print('Loading GrooveTransformer models...')
            model_index = 1
            assert model_index in range(1, 5), "model_index must be in range 1 to 4"
            model_path = "modes/GrooveTransformerExample/model/checkpoints/model{}.pth".format(model_index)
            self.gt_model = load_mgt_model(model_path)

    def generate_sequence(self):
        if self.gt_model is not None:

            input_groove_hits = [round(np.random.rand(), 0) for _ in range(32)]
            input_groove_velocities = [round(np.random.random(), 2) if input_groove_hits[ix] == 1 else 0 for ix in range(32)]
            input_groove_offsets = [round(np.random.random() - 0.5, 2) if input_groove_hits[ix] == 1 else 0 for ix in range(32)]

            input_clip = self.clip_edit_mode.clip
            if input_clip is not None and input_clip.clip_length_in_beats is not 0.0 and len(input_clip.sequence_events) > 0:
                '''
                [1.0, 1.0, 1.0, 0.0, 0.0, 1.0, 1.0, 0.0, 1.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 0.0, 1.0, 1.0, 0.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 0.0, 1.0, 1.0, 0.0]
                [0.12, 0.96, 0.73, 0, 0, 0.6, 0.7, 0, 0.22, 0, 0.46, 0, 0.55, 0.51, 0, 0, 0.41, 0.55, 0, 0.31, 0.64, 0.62, 0.32, 0.0, 0.91, 0.32, 0.76, 0.43, 0, 0.24, 0.2, 0]
                [-0.31, 0.28, 0.17, 0, 0, 0.17, -0.25, 0, -0.44, 0, 0.23, 0, -0.24, -0.01, 0, 0, -0.47, -0.39, 0, 0.04, -0.38, 0.39, -0.12, 0.26, -0.06, 0.26, 0.49, -0.04, 0, -0.28, -0.49, 0]
                '''
                input_groove_hits = [0.0 for _ in range(32)]
                input_groove_velocities = [0.0 for _ in range(32)]
                input_groove_offsets  = [0.0 for _ in range(32)]
                
                for sequence_event in input_clip.sequence_events:
                    timestamp_in_32_scale = sequence_event.timestamp * 4.0
                    timestamp_position_in_tensor = int(round(timestamp_in_32_scale))
                    if timestamp_position_in_tensor < 32:
                        input_groove_hits[timestamp_position_in_tensor] = 1.0
                        input_groove_velocities[timestamp_position_in_tensor] = 1.0
                        input_groove_offsets[timestamp_position_in_tensor] = timestamp_position_in_tensor - timestamp_in_32_scale  # Not sure what units I should use here for offset (is it 1/32 of the beat?)
                    else:
                        pass  # Ignore sequence events beyond the 8 beats (32.0 in 32 times scale)
                
            h, v, o = predict_using_h_v_o(
                trained_model=self.gt_model,
                input_h=input_groove_hits,
                input_v=input_groove_velocities,
                input_o=input_groove_offsets,
                voice_thresholds=[1.0 - self.parameters['threshold']['value'] for _ in range(0, len(self.sampling_thresholds))],  #self.sampling_thresholds.values(),
                voice_max_count_allowed=self.max_counts_allowed.values())

            notes = strip_note_from_hvo(h=h, v=v, o=o, drum_map=self.drum_mappings)
            sequence_length = 8.0
            sequence_notes = []
            for note in notes:
                sequence_notes.append(
                    {'type': 1, 
                     'midiNote': int(note['pitch'][0]), 
                     'midiVelocity': float(note['velocity'][0]), 
                     'timestamp': float(note['quantizedTime'][0]),
                     'uTime': float(note['offset'][0]) / 4.0,
                     'duration': 0.1})
            return sequence_notes, sequence_length

        else:
            print('Can\'t generate new sequence with GrooveTransfomer as pytorch is not available')
            return None, None
    