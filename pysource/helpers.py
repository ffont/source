class PlStateNames:
    SYSTEM_STATS = 'SYSTEM_STATS'
    CONNECTION_WITH_PLUGIN_OK = 'CONNECTION_WITH_PLUGIN_OK'
    NETWORK_IS_CONNECTED = 'NETWORK_IS_CONNECTED'

    SOURCE_DATA_LOCATION = 'sourceDataLocation'
    SOUNDS_DATA_LOCATION = 'soundsDownloadLocation'
    PRESETS_DATA_LOCATION = 'presetFilesLocation'
    TMP_DATA_LOCATION = 'tmpFilesLocation'

    PLUGIN_VERSION = 'pluginVersion'

    USE_ORIGINAL_FILES_PREFERENCE = 'useOriginalFilesPreference'
    FREESOUND_OAUTH_TOKEN = 'freesoundOauthAccessToken'
    MIDI_IN_CHANNEL = 'globalMidiInChannel'

    LOADED_PRESET_NAME = 'name_preset'

    LOADED_PRESET_INDEX = 'currentPresetIndex'
    NUM_VOICES = 'numVoices'
    NOTE_LAYOUT_TYPE = 'noteLayoutType'

    NUM_SOUNDS = 'NUM_SOUNDS'
    NUM_SOUNDS_DOWNLOADING = 'NUM_SOUNDS_DOWNLOADING'
    NUM_SOUNDS_LOADED_IN_SAMPLER = 'NUM_SOUNDS_LOADED_IN_SAMPLER'

    NAME = 'name'
    UUID = 'uuid'

    SOUND_NAME = 'name_sound'
    SOUND_UUID = 'uuid_sound'

    SOURCE_SAMPLER_SOUND_UUID = 'uuid_source_sampler_sound'
    SOUND_ID = 'soundId'
    SOUND_LICENSE = 'license'
    SOUND_AUTHOR = 'username'
    SOUND_DURATION = 'duration'
    SOUND_DOWNLOAD_PROGRESS = 'downloadProgress'
    SOUND_DOWNLOAD_COMPLETED = 'downloadCompleted'
    SOUND_OGG_URL = 'previewURL'
    SOUND_LOCAL_FILE_PATH = 'filePath'
    SOUND_TYPE = 'format'
    SOUND_FILESIZE = 'filesize'
    SOUND_LOADED_PREVIEW_VERSION = 'usesPreview'
    SOUND_SLICES = 'SOUND_SLICES'
    SOUND_ASSIGNED_NOTES = 'midiNotes'
    SOUND_LOADED_IN_SAMPLER = 'allSoundsLoaded'

    SOUND_MIDI_CC_ASSIGNMENTS = 'SOUND_MIDI_CC_ASSIGNMENTS'
    SOUND_MIDI_CC_ASSIGNMENT_CC_NUMBER = 'SOUND_MIDI_CC_ASSIGNMENT_CC_NUMBER'
    SOUND_MIDI_CC_ASSIGNMENT_PARAM_NAME = 'SOUND_MIDI_CC_ASSIGNMENT_PARAM_NAME'
    SOUND_MIDI_CC_ASSIGNMENT_MIN_RANGE = 'SOUND_MIDI_CC_ASSIGNMENT_MIN_RANGE'
    SOUND_MIDI_CC_ASSIGNMENT_MAX_RANGE = 'SOUND_MIDI_CC_ASSIGNMENT_MAX_RANGE'
    SOUND_MIDI_CC_ASSIGNMENT_UUID = 'SOUND_MIDI_CC_ASSIGNMENT_UUID'

    REVERB_SETTINGS = 'REVERB_SETTINGS'

    METER_L = 'METER_L'
    METER_R = 'METER_R'
    IS_QUERYING = 'IS_QUERYING'
    VOICE_SOUND_IDXS = 'VOICE_SOUND_IDXS'
    NUM_ACTIVE_VOICES = 'NUM_ACTIVE_VOICES'
    MIDI_RECEIVED = 'MIDI_RECEIVED'
    LAST_CC_MIDI_RECEIVED = 'LAST_CC_MIDI_RECEIVED'
    LAST_NOTE_MIDI_RECEIVED = 'LAST_NOTE_MIDI_RECEIVED'


state_names_source_state_hierarchy_map = {
    PlStateNames.SYSTEM_STATS: 'extra_state',
    PlStateNames.CONNECTION_WITH_PLUGIN_OK: 'extra_state',
    PlStateNames.NETWORK_IS_CONNECTED: 'extra_state',

    PlStateNames.SOURCE_DATA_LOCATION: 'source_state',
    PlStateNames.SOUNDS_DATA_LOCATION: 'source_state',
    PlStateNames.PRESETS_DATA_LOCATION: 'source_state',
    PlStateNames.TMP_DATA_LOCATION: 'source_state',

    PlStateNames.PLUGIN_VERSION: 'source_state',
    PlStateNames.USE_ORIGINAL_FILES_PREFERENCE: 'source_state',
    PlStateNames.FREESOUND_OAUTH_TOKEN: 'source_state',
    PlStateNames.MIDI_IN_CHANNEL: 'source_state',

    PlStateNames.LOADED_PRESET_NAME: 'preset',
    PlStateNames.LOADED_PRESET_INDEX: 'source_state',
    PlStateNames.NUM_VOICES: 'preset',
    PlStateNames.NOTE_LAYOUT_TYPE: 'preset',

    PlStateNames.NUM_SOUNDS: 'computed',
    PlStateNames.NUM_SOUNDS_DOWNLOADING: 'computed',
    PlStateNames.NUM_SOUNDS_LOADED_IN_SAMPLER: 'computed',

    PlStateNames.SOUND_NAME: 'sound_sample',
    PlStateNames.SOUND_UUID: 'sound',

    PlStateNames.SOURCE_SAMPLER_SOUND_UUID: 'sound_sample',
    PlStateNames.SOUND_ID: 'sound_sample',
    PlStateNames.SOUND_LICENSE: 'sound_sample',
    PlStateNames.SOUND_AUTHOR: 'sound_sample',
    PlStateNames.SOUND_DURATION: 'sound_sample',
    PlStateNames.SOUND_DOWNLOAD_PROGRESS: 'sound_sample',
    PlStateNames.SOUND_DOWNLOAD_COMPLETED: 'sound_sample',
    PlStateNames.SOUND_OGG_URL: 'sound_sample',
    PlStateNames.SOUND_LOCAL_FILE_PATH: 'sound_sample',
    PlStateNames.SOUND_TYPE: 'sound_sample',
    PlStateNames.SOUND_FILESIZE: 'sound_sample',
    PlStateNames.SOUND_LOADED_PREVIEW_VERSION: 'sound_sample',

    PlStateNames.SOUND_SLICES: 'computed',
    PlStateNames.SOUND_ASSIGNED_NOTES: 'sound',
    PlStateNames.SOUND_LOADED_IN_SAMPLER: 'sound',

    PlStateNames.SOUND_MIDI_CC_ASSIGNMENTS: 'computed',
    PlStateNames.SOUND_MIDI_CC_ASSIGNMENT_CC_NUMBER: None,
    PlStateNames.SOUND_MIDI_CC_ASSIGNMENT_PARAM_NAME: None,
    PlStateNames.SOUND_MIDI_CC_ASSIGNMENT_MIN_RANGE: None,
    PlStateNames.SOUND_MIDI_CC_ASSIGNMENT_MAX_RANGE: None,
    PlStateNames.SOUND_MIDI_CC_ASSIGNMENT_UUID: None,

    PlStateNames.REVERB_SETTINGS: 'computed',

    PlStateNames.METER_L: 'volatile',
    PlStateNames.METER_R: 'volatile',
    PlStateNames.IS_QUERYING: 'volatile',
    PlStateNames.VOICE_SOUND_IDXS: 'volatile',
    PlStateNames.NUM_ACTIVE_VOICES: 'volatile',
    PlStateNames.MIDI_RECEIVED: 'volatile',
    PlStateNames.LAST_CC_MIDI_RECEIVED: 'volatile',
    PlStateNames.LAST_NOTE_MIDI_RECEIVED: 'volatile',
}

def snap_to_value(x, value=0.5, margin=0.07):
    if abs(x - value) < margin:
        return value
    return x

def lin_to_exp(x):
    sign = 1 if x >= 0 else -1
    return sign * pow(x, 2)

#  send_func (norm to val), get_func (val to val), parameter_label, value_label_template, set osc address
sound_parameters_info_dict = {
    "gain": (lambda x: 12.0 * lin_to_exp(2.0 * (snap_to_value(x) - 0.5)) if x >= 0.5 else 36.0 * lin_to_exp(2.0 * (snap_to_value(x) - 0.5)), lambda x: float(x), "Gain", "{0:.2f}dB", "/set_sound_parameter"),
    "pitch": (lambda x: 36.0 * lin_to_exp(2.0 * (snap_to_value(x) - 0.5)), lambda x: float(x), "Pitch", "{0:.2f}", "/set_sound_parameter"),
    "reverse": (lambda x: int(round(x)), lambda x: ['Off', 'On'][int(x)], "Reverse", "{0}", "/set_sound_parameter_int"),
    "launchMode": (lambda x: int(round(4 * x)), lambda x: ['Gate', 'Loop', 'Ping-pong', 'Trigger', 'Freeze'][int(x)], "Launch mode", "{0}", "/set_sound_parameter_int"),
    "startPosition": (lambda x: x, lambda x: float(x), "Start pos", "{0:.4f}", "/set_sound_parameter"),
    "endPosition": (lambda x: x, lambda x: float(x), "End pos", "{0:.4f}", "/set_sound_parameter"),
    "loopStartPosition": (lambda x: x, lambda x: float(x), "Loop st pos", "{0:.4f}", "/set_sound_parameter"),
    "loopEndPosition": (lambda x: x, lambda x: float(x), "Loop end pos", " {0:.4f}", "/set_sound_parameter"),
    "attack": (lambda x: 20.0 * lin_to_exp(x), lambda x: float(x), "Attack", "{0:.2f}s", "/set_sound_parameter"),
    "decay": (lambda x: 20.0 * lin_to_exp(x), lambda x: float(x), "Decay", "{0:.2f}s", "/set_sound_parameter"),
    "sustain": (lambda x: x, lambda x: float(x), "Sustain", "{0:.2f}", "/set_sound_parameter"),
    "release": (lambda x: 20.0 * lin_to_exp(x), lambda x: float(x), "Release", "{0:.2f}s", "/set_sound_parameter"),
    "filterCutoff": (lambda x: 10 + 20000 * lin_to_exp(x), lambda x: float(x), "Cutoff", "{0:.2f}Hz", "/set_sound_parameter"),
    "filterRessonance": (lambda x: x, lambda x: float(x), "Resso", "{0:.2f}", "/set_sound_parameter"),
    "filterKeyboardTracking": (lambda x: x, lambda x: float(x), "K.T.", "{0:.2f}", "/set_sound_parameter"),
    "filterADSR2CutoffAmt": (lambda x: 100.0 * x, lambda x: float(x), "Env amt", "{0:.0f}%", "/set_sound_parameter"),
    "filterAttack": (lambda x: 20.0 * lin_to_exp(x), lambda x: float(x), "Filter Attack", "{0:.2f}s", "/set_sound_parameter"),
    "filterDecay": (lambda x: 20.0 * lin_to_exp(x), lambda x: float(x), "Filter Decay", "{0:.2f}s", "/set_sound_parameter"),
    "filterSustain": (lambda x: x, lambda x: float(x), "Filter Sustain", "{0:.2f}", "/set_sound_parameter"),
    "filterRelease": (lambda x: 20.0 * lin_to_exp(x), lambda x: float(x), "Filter Release", "{0:.2f}s", "/set_sound_parameter"),
    "noteMappingMode": (lambda x: int(round(3 * x)), lambda x: ['Pitch', 'Slice', 'Both', 'Repeat'][int(x)], "Map mode", "{0}", "/set_sound_parameter_int"),
    "numSlices": (lambda x: int(round(32.0 * x)), lambda x: (['Auto onsets', 'Auto notes']+[str(x) for x in range(2, 101)])[int(x)], "# slices", "{0}", "/set_sound_parameter_int"),
    "playheadPosition": (lambda x: x, lambda x: float(x), "Playhead", "{0:.4f}", "/set_sound_parameter"),
    "freezePlayheadSpeed": (lambda x: 1 + 4999 * lin_to_exp(x), lambda x: float(x), "Freeze speed", "{0:.1f}", "/set_sound_parameter"),
    "pitchBendRangeUp": (lambda x: 36.0 * x, lambda x: float(x), "P.Bend down", "{0:.1f}st", "/set_sound_parameter"),
    "pitchBendRangeDown": (lambda x: 36.0 * x, lambda x: float(x), "P.Bend up", "{0:.1f}st", "/set_sound_parameter"),
    "mod2CutoffAmt": (lambda x: 100.0 * x, lambda x: float(x), "Mod2Cutoff", "{0:.0f}%", "/set_sound_parameter"),
    "mod2GainAmt": (lambda x: 12.0 * lin_to_exp(2.0 * (snap_to_value(x) - 0.5)), lambda x: float(x), "Mod2Gain", "{0:.1f}dB", "/set_sound_parameter"),
    "mod2PitchAmt": (lambda x: 12.0 * lin_to_exp(2.0 * (snap_to_value(x) - 0.5)), lambda x: float(x), "Mod2Pitch", "{0:.2f}st", "/set_sound_parameter"),
    "mod2PlayheadPos": (lambda x: x, lambda x: float(x), "Mod2PlayheadPos", "{0:.2f}", "/set_sound_parameter"),
    "vel2CutoffAmt": (lambda x: 100.0 * x, lambda x: float(x), "Vel2Cutoff", "{0:.0f}%", "/set_sound_parameter"),
    "vel2GainAmt": (lambda x: x, lambda x: int(100 * float(x)), "Vel2Gain", "{0}%", "/set_sound_parameter"),
    "velSensitivity": (lambda x: 6.0 * x, lambda x: float(x), "VelSensitivity", "{0:.2f}", "/set_sound_parameter"),
    "pan": (lambda x: 2.0 * (snap_to_value(x) - 0.5), lambda x: float(x), "Panning", "{0:.1f}", "/set_sound_parameter"),
    "loopXFadeNSamples": (lambda x: 10 + int(round(lin_to_exp(x) * (100000 - 10))), lambda x: int(x), "Loop X fade len", "{0}", "/set_sound_parameter_int"),
    "midiChannel": (lambda x: int(round(16 * x)), lambda x: ['Global', "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16"][int(x)], "MIDI channel", "{0}", "/set_sound_parameter_int"),
}

LICENSE_UNKNOWN = 'Unknown'
LICENSE_CC0 = 'CC0'
LICENSE_CC_BY = 'CC-BY'
LICENSE_CC_BY_SA = 'CC-BY-SA'
LICENSE_CC_BY_NC = 'CC-BY-NC'
LICENSE_CC_BY_ND = 'CC-BY-ND'
LICENSE_CC_BY_NC_SA = 'CC-BY-NC-SA'
LICENSE_CC_BY_NC_ND = 'CC-BY-NC-ND'
LICENSE_CC_SAMPLING_PLUS = 'Sampling+'


def translate_cc_license_url(url):
    if '/by/' in url: return LICENSE_CC_BY
    if '/by-nc/' in url: return LICENSE_CC_BY_NC
    if '/by-nd/' in url: return LICENSE_CC_BY_ND
    if '/by-sa/' in url: return LICENSE_CC_BY_SA
    if '/by-nc-sa/' in url: return LICENSE_CC_BY_NC_SA
    if '/by-nc-nd/' in url: return LICENSE_CC_BY_NC_ND
    if '/zero/' in url: return LICENSE_CC0
    if '/publicdomain/' in url: return LICENSE_CC0
    if '/sampling+/' in url: return LICENSE_CC_SAMPLING_PLUS
    return LICENSE_UNKNOWN