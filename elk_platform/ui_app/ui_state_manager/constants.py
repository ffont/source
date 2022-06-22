EXTRA_PAGE_1_NAME = "extra1"
EXTRA_PAGE_2_NAME = "extra2"
sound_parameter_pages = [
    [
        "gain",
        "pitch",
        "reverse",
        "launchMode",
    ], [
        "noteMappingMode",
        "numSlices",
        "playheadPosition",
        "freezePlayheadSpeed",
    ], [
        "startPosition",
        "endPosition",
        "loopStartPosition",
        "loopEndPosition",
    ], [
        "attack",
        "decay",
        "sustain",
        "release",
    ], [
        "filterCutoff",
        "filterRessonance",
        "filterKeyboardTracking",
        "filterADSR2CutoffAmt",
    ], [
        "filterAttack",
        "filterDecay",
        "filterSustain",
        "filterRelease",
    ], [
        "vel2CutoffAmt",
        "vel2GainAmt",
        "velSensitivity",
        "pan",
    ], [
        "mod2CutoffAmt",
        "mod2GainAmt",
        "mod2PitchAmt",
        "mod2PlayheadPos",
    ], [
        "pitchBendRangeUp",
        "pitchBendRangeDown",
        "loopXFadeNSamples",
        "midiChannel",
    ]
]

#  parameter position in OSC message, parameter label
reverb_parameters_info_dict = {
    "roomSize": (0, "Room size"),
    "damping": (1, "Damping"),
    "wetLevel": (2, "Wet level"),
    "dryLevel": (3, "Dry level"),
    "width": (4, "Width"),
    "freezeMode": (5, "Freeze"),
}

note_layout_types = ['Contiguous', 'Interleaved']
license_types = ['All', 'CC0', 'Exclude NC']
num_sounds_options = [1, 2, 3, 4, 5, 6, 8, 12, 16, 24, 32, 64]
ac_descriptors_options = ['off', 'low', 'mid', 'high']
ac_descriptors_names = ['brightness', 'hardness', 'depth', 'roughness','boominess', 'warmth', 'sharpness']

query_settings_info_dict = {
    'num_sounds': (lambda x:num_sounds_options[int(round(x * (len(num_sounds_options) - 1)))], 'Num sounds', '{}', 16),
    'min_length': (lambda x:float(pow(x, 4) * 300), 'Min length', '{0:.1f}s', 0.0),
    'max_length': (lambda x:float(pow(x, 4) * 300), 'Max length', '{0:.1f}s', 10.0),
    'layout': (lambda x:note_layout_types[int(round(x * (len(note_layout_types) - 1)))], 'Layout', '{}', note_layout_types[1]),
    'brightness': (lambda x:ac_descriptors_options[int(round(x * (len(ac_descriptors_options) - 1)))], 'Brightness', '{}', ac_descriptors_options[0]),
    'hardness': (lambda x:ac_descriptors_options[int(round(x * (len(ac_descriptors_options) - 1)))], 'Hardness', '{}', ac_descriptors_options[0]),
    'depth': (lambda x:ac_descriptors_options[int(round(x * (len(ac_descriptors_options) - 1)))], 'Depth', '{}', ac_descriptors_options[0]),
    'roughness': (lambda x:ac_descriptors_options[int(round(x * (len(ac_descriptors_options) - 1)))], 'Roughness', '{}', ac_descriptors_options[0]),
    'boominess': (lambda x:ac_descriptors_options[int(round(x * (len(ac_descriptors_options) - 1)))], 'Boominess', '{}', ac_descriptors_options[0]),
    'warmth': (lambda x:ac_descriptors_options[int(round(x * (len(ac_descriptors_options) - 1)))], 'Warmth', '{}', ac_descriptors_options[0]),
    'sharpness': (lambda x:ac_descriptors_options[int(round(x * (len(ac_descriptors_options) - 1)))], 'Sharpness', '{}', ac_descriptors_options[0]),
    'license':  (lambda x:license_types[int(round(x * (len(license_types) - 1)))], 'License', '{}', license_types[0]),
}

query_settings_pages = [
    [
        'num_sounds',
        'min_length',
        'max_length',
        'layout'
    ], [
        'brightness',
        'hardness',
        'depth',
        'roughness'
    ], [
        'boominess',
        'warmth',
        'sharpness',
        'license'
    ]
]

midi_cc_available_parameters_list = ["startPosition", "endPosition", "loopStartPosition", "loopEndPosition", "playheadPosition", "freezePlayheadSpeed", "filterCutoff", "filterRessonance", "gain", "pan", "pitch"]

predefined_queries = ['wind', 'rain', 'piano', 'explosion', 'music', 'whoosh', 'woosh', 'intro', 'birds', 'footsteps', 'fire', '808', 'scream', 'water', 'bell', 'click', 'thunder', 'guitar', 'bass', 'beep', 'swoosh', 'pop', 'cartoon', 'magic', 'car', 'horror', 'vocal', 'game', 'trap', 'lofi', 'clap', 'happy', 'forest', 'ding', 'drum', 'kick', 'glitch', 'drop', 'transition', 'animal', 'gun', 'door', 'hit', 'punch', 'nature', 'jump', 'flute', 'sad', 'beat', 'christmas']
predefined_queries = sorted(predefined_queries)

reverb_parameters_pages = [
    [
        "roomSize",
        "damping",
        "width",
        "freezeMode"
    ], [
        "wetLevel",
        "dryLevel",
        None,
        None
    ]
]