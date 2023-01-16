import random

from utils import clamp


class GeneratorAlogorithm(object):
    parameters = []
    name = ''

    def __init__(self):
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
    name = 'Rnd'
    parameters = {
        'length': {'display_name': 'LENGTH', 'type': float, 'min': 1.0, 'max': 32.0, 'default': 8.0,
                   'increment_scale': 1.0},
        'density': {'display_name': 'DENSITY', 'type': int, 'min': 1, 'max': 15, 'default': 5, 'increment_scale': 1},
    }

    def generate_sequence(self):
        if self.parameters['length']['value'] > 0.0:
            new_clip_length = self.parameters['length']['value']
        else:
            new_clip_length = random.randint(5, 13)
        random_sequence = []
        for i in range(0, abs(self.parameters['density']['value'] + random.randint(-2, 2))):
            timestamp = (new_clip_length - 0.5) * random.random()
            duration = random.random() * 1.5 + 0.01
            random_sequence.append(
                {'type': 1, 'midiNote': random.randint(64, 85), 'midiVelocity': 1.0, 'timestamp': timestamp,
                 'duration': duration}
            )
        return random_sequence, new_clip_length


class RandomGeneratorAlgorithmPlus(GeneratorAlogorithm):
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
