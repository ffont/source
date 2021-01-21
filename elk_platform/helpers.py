import numpy
import threading

from collections import defaultdict
from enum import Enum, auto
from functools import wraps
from PIL import ImageFont, Image, ImageDraw

# -- Porcessed state names

class StateNames(Enum):

    SYSTEM_STATS = auto()

    SOURCE_DATA_LOCATION = auto()
    SOUNDS_DATA_LOCATION = auto()
    PRESETS_DATA_LOCATION = auto()

    STATE_UPDATED_RECENTLY = auto()
    CONNECTION_WITH_PLUGIN_OK = auto()
    NETWORK_IS_CONNECTED = auto()
    
    IS_QUERYING_AND_DOWNLOADING = auto()
    
    LOADED_PRESET_NAME = auto()
    LOADED_PRESET_INDEX = auto()
    NUM_VOICES = auto()
    NOTE_LAYOUT_TYPE = auto()
    
    NUM_SOUNDS = auto()
    NUM_SOUNDS_DOWNLOADING = auto()
    NUM_SOUNDS_LOADED_IN_SAMPLER = auto()
    SOUNDS_INFO = auto()
    SOUND_NAME = auto()
    SOUND_ID = auto()
    SOUND_LICENSE = auto()
    SOUND_AUTHOR = auto()
    SOUND_DURATION = auto()
    SOUND_DOWNLOAD_PROGRESS = auto()
    SOUND_PARAMETERS = auto()
    SOUND_OGG_URL = auto()
    SOUND_SLICES = auto()
    SOUND_ASSIGNED_NOTES = auto()
    SOUND_LOADED_IN_SAMPLER = auto()
    
    SOUND_MIDI_CC_ASSIGNMENTS = auto()
    SOUND_MIDI_CC_ASSIGNMENT_CC_NUMBER = auto()
    SOUND_MIDI_CC_ASSIGNMENT_PARAM_NAME = auto()
    SOUND_MIDI_CC_ASSIGNMENT_MIN_RANGE = auto()
    SOUND_MIDI_CC_ASSIGNMENT_MAX_RANGE = auto()
    SOUND_MIDI_CC_ASSIGNMENT_RANDOM_ID = auto()

    REVERB_SETTINGS = auto()

    NUM_ACTIVE_VOICES = auto()
    MIDI_RECEIVED = auto()
    LAST_CC_MIDI_RECEIVED = auto()
    LAST_NOTE_MIDI_RECEIVED = auto()
    

def process_xml_state_from_plugin(plugin_state_xml, sound_parameters_info_dict):
    source_state = {}
            
    # Get sub XML element to avoid repeating many queries
    volatile_state = plugin_state_xml.find_all("VolatileState".lower())[0]
    preset_state = plugin_state_xml.find_all("SourcePresetState".lower())[0]
    sampler_state = preset_state.find_all("Sampler".lower())[0]

    # Source settings
    source_state[StateNames.SOURCE_DATA_LOCATION] = volatile_state.get('sourceDataLocation'.lower(), None)
    source_state[StateNames.SOUNDS_DATA_LOCATION] = volatile_state.get('soundsDataLocation'.lower(), None)
    source_state[StateNames.PRESETS_DATA_LOCATION] = volatile_state.get('presetsDataLocation'.lower(), None)
    
    # Is plugin currently querying and downloading?
    source_state[StateNames.IS_QUERYING_AND_DOWNLOADING] = volatile_state.get('isQueryingAndDownloadingSounds'.lower(), '') != "0"

    # Basic preset properties
    source_state[StateNames.LOADED_PRESET_NAME] = preset_state.get("presetName".lower(), "Noname")
    source_state[StateNames.LOADED_PRESET_INDEX] = int(preset_state.get("presetNumber".lower(), "-1"))
    source_state[StateNames.NUM_VOICES] = int(sampler_state.get("NumVoices".lower(), "1"))
    source_state[StateNames.NOTE_LAYOUT_TYPE] = int(preset_state.get("noteLayoutType".lower(), "1"))

    # Reverb settings
    reverb_state = plugin_state_xml.find_all("ReverbParameters".lower())[0]
    source_state[StateNames.REVERB_SETTINGS] = [
        float(reverb_state.get('reverb_roomSize'.lower(), 0.0)),
        float(reverb_state.get('reverb_damping'.lower(), 0.0)),
        float(reverb_state.get('reverb_wetLevel'.lower(), 0.0)),
        float(reverb_state.get('reverb_dryLevel'.lower(), 0.0)),
        float(reverb_state.get('reverb_width'.lower(), 0.0)),
        float(reverb_state.get('reverb_freezeMode'.lower(), 0.0)),
    ]
    
    # Loaded sounds properties
    sounds_info = preset_state.find_all("soundsInfo".lower())[0].find_all("soundInfo".lower())
    source_state[StateNames.NUM_SOUNDS] = len(sounds_info)
    processed_sounds_info = []
    for sound_info in sounds_info:

        sampler_sound = sound_info.find_all("SamplerSound".lower())[0]
        sound_parameters = sampler_sound.find_all('SamplerSoundParameter'.lower())

        # Consolidate sound slices into a list
        slices = []
        for onset in sound_info.find_all('onset'):
            slices.append(float(onset['time']))

        # Consolidate all sound parameter values into a dict
        processed_sound_parameters_info = {}
        for parameter in sound_parameters:
            """
            <SamplerSoundParameter parameter_type="int" parameter_name="launchMode" parameter_value="1"/>
            """
            val = parameter['parameter_value']
            name = parameter['parameter_name']
            processed_sound_parameters_info[name] = val

        # Concoslidate MIDI CC mapping data into a dict
        # NOTE: We use some complex logic here in order to allow several mappings with the same CC#/Parameter name
        processed_sound_midi_cc_info = {}
        processed_sound_midi_cc_info_list_aux = []
        for midi_cc in sampler_sound.find_all('SamplerSoundMidiCCMapping'.lower()):
            """
            <SamplerSoundMidiCCMapping
                midiMappingRandomId="14562" 
                midiMappingCcNumber="2" 
                midiMappingParameterName="filterCutoff"
                midiMappingMinRange="0.0" 
                midiMappingMaxRange="1.0"
            />
            """
            cc_number = int(midi_cc['midiMappingCcNumber'.lower()])
            parameter_name = midi_cc['midiMappingParameterName'.lower()]
            label = 'CC#{}->{}'.format(cc_number, sound_parameters_info_dict[parameter_name][2])  # Be careful, if sound_parameters_info_dict structure changes, this won't work
            processed_sound_midi_cc_info_list_aux.append((label, {
                StateNames.SOUND_MIDI_CC_ASSIGNMENT_PARAM_NAME: parameter_name,
                StateNames.SOUND_MIDI_CC_ASSIGNMENT_CC_NUMBER: cc_number,
                StateNames.SOUND_MIDI_CC_ASSIGNMENT_MIN_RANGE: float(midi_cc['midiMappingMinRange'.lower()]),
                StateNames.SOUND_MIDI_CC_ASSIGNMENT_MAX_RANGE: float(midi_cc['midiMappingMaxRange'.lower()]),
                StateNames.SOUND_MIDI_CC_ASSIGNMENT_RANDOM_ID: int(midi_cc['midiMappingRandomId'.lower()]) ,
            }))
        # Sort by assignment random ID and then for label. In this way the sorting will be always consistent
        processed_sound_midi_cc_info_list_aux = sorted(processed_sound_midi_cc_info_list_aux, key=lambda x:x[1][StateNames.SOUND_MIDI_CC_ASSIGNMENT_RANDOM_ID])
        processed_sound_midi_cc_info_list_aux = sorted(processed_sound_midi_cc_info_list_aux, key=lambda x:x[0])
        assignment_labels = [label for label, _ in processed_sound_midi_cc_info_list_aux]
        added_labels_count = defaultdict(int)
        for assignment_label, assignment_data in processed_sound_midi_cc_info_list_aux:
            added_labels_count[assignment_label] += 1
            if added_labels_count[assignment_label] > 1:
                assignment_label = '{0} ({1})'.format(assignment_label, added_labels_count[assignment_label])
            processed_sound_midi_cc_info[assignment_label] = assignment_data

        # Put all sound data together into a dict
        processed_sounds_info.append({
            StateNames.SOUND_NAME: sound_info.get('soundname', '-'),
            StateNames.SOUND_ID: sound_info.get('soundid', 0),
            StateNames.SOUND_LICENSE: translate_cc_license_url(sound_info.get('soundlicense', '-')),
            StateNames.SOUND_AUTHOR: sound_info.get('sounduser', '-'),
            StateNames.SOUND_DURATION: float(sound_info.get('sounddurationinseconds', 0)),
            StateNames.SOUND_OGG_URL: sound_info.get('soundoggurl', ''),
            StateNames.SOUND_DOWNLOAD_PROGRESS: '{0}'.format(int(sound_info.get('downloadprogress', 0))),
            StateNames.SOUND_PARAMETERS: processed_sound_parameters_info,
            StateNames.SOUND_SLICES: slices,
            StateNames.SOUND_ASSIGNED_NOTES: sampler_sound.get('midiNotes'.lower(), None),
            StateNames.SOUND_MIDI_CC_ASSIGNMENTS: processed_sound_midi_cc_info,
            StateNames.SOUND_LOADED_IN_SAMPLER: len(sound_parameters) > 0,
        })

    source_state[StateNames.SOUNDS_INFO] = processed_sounds_info
    source_state[StateNames.NUM_SOUNDS_LOADED_IN_SAMPLER] = len([s for s in processed_sounds_info if s[StateNames.SOUND_LOADED_IN_SAMPLER]])
    source_state[StateNames.NUM_SOUNDS_DOWNLOADING] = len([s for s in processed_sounds_info if int(s[StateNames.SOUND_DOWNLOAD_PROGRESS]) < 100])

    # More volatile state stuff
    source_state[StateNames.NUM_ACTIVE_VOICES] = sum([int(element) for element in volatile_state.get('voiceActivations'.lower(), '').split(',') if element])
    source_state[StateNames.MIDI_RECEIVED] = "1" == volatile_state.get('midiInLastStateReportBlock'.lower(), "0")
    source_state[StateNames.LAST_CC_MIDI_RECEIVED] = int(volatile_state.get('lastMIDICCNumber'.lower(), -1))
    source_state[StateNames.LAST_NOTE_MIDI_RECEIVED] = int(volatile_state.get('lastMIDINoteNumber'.lower(), -1))

    return source_state


# -- UI helpers

DISPLAY_SIZE = (128, 64)
FONT_PATH = 'LiberationMono-Regular.ttf'
FONT_SIZE = 10
FONT_SIZE_BIG = 25
FONT_PATH_TITLE = 'FuturaHeavyfont.ttf'
FONT_SIZE_TITLE = 64 - 10
RA_LOGO_PATH = 'logo_oled.png'
RA_LOGO_B_PATH = 'logo_oled_b.png'
FS_LOGO_PATH = 'logo_oled_fs.png'
TITLE_TEXT = '                             SOURCE, by Rita & Aurora                                    '
START_ANIMATION_DURATION = 8

font = ImageFont.truetype(FONT_PATH, FONT_SIZE)
font_heihgt_px = FONT_SIZE
font_width_px = font_heihgt_px * 0.6   # This is particular to LiberationMono font
n_chars_in_line = int(DISPLAY_SIZE[0]/font_width_px)

font_big = ImageFont.truetype(FONT_PATH, FONT_SIZE_BIG)
font_big_heihgt_px = FONT_SIZE_BIG
font_big_width_px = font_big_heihgt_px * 0.6   # This is particular to LiberationMono font

title_text_width = None
title_font = ImageFont.truetype(FONT_PATH_TITLE, FONT_SIZE_TITLE)
ra_logo = Image.open(RA_LOGO_PATH).resize(DISPLAY_SIZE, Image.NEAREST).convert('1')
ra_logo_b = Image.open(RA_LOGO_B_PATH).resize(DISPLAY_SIZE, Image.NEAREST).convert('1')
fs_logo = Image.open(FS_LOGO_PATH).resize(DISPLAY_SIZE, Image.NEAREST).convert('1')


def justify_text(left_bit, right_bit, total_len=n_chars_in_line):
    len_right = min(len(right_bit), total_len)
    len_left = min(len(left_bit), total_len - len_right)
    pad_middle = n_chars_in_line - len_right - len_left
    return "{0}{1}{2}".format(left_bit[0:len_left], " " * pad_middle, right_bit[0:len_right])


def frame_from_lines(lines_info):
    im = Image.new(mode='1', size=DISPLAY_SIZE)
    draw = ImageDraw.Draw(im)

    y_offset = 0
    for count, line in enumerate(lines_info):
        if type(line) == str:
            draw.text((1, y_offset), line, font=font, fill="white")
        elif type(line) == dict:
            invert_colors = line.get('invert', False)
            underline = line.get('underline', False)
            text_color = "white"
            if invert_colors:
                draw.rectangle((0, y_offset, DISPLAY_SIZE[0], y_offset + font_heihgt_px), outline="white", fill="white")
                text_color = "black"
            if underline:
                draw.line((0, y_offset + font_heihgt_px, DISPLAY_SIZE[0], y_offset + font_heihgt_px), fill="white")
            draw.text( (1, y_offset), line['text'], font=font, fill=text_color)

        y_offset += font_heihgt_px + 1     

    return im


def frame_from_start_animation(progress, counter):
    global title_text_width

    im = Image.new(mode='1', size=DISPLAY_SIZE)
    draw = ImageDraw.Draw(im)
    if progress < 0.3:
        # Show logo
        if counter % 2 == 0:
            draw.bitmap((0, 0), ra_logo, fill="white")
        else:
            draw.bitmap((0, 0), ra_logo_b, fill="white")
    elif 0.3 <= progress <= 0.7:
        # Show text
        if title_text_width is None:
            title_text_width = draw.textsize(TITLE_TEXT, font=title_font)[0]
        draw.text((int(progress * -1 * title_text_width), int((DISPLAY_SIZE[1] - FONT_SIZE_TITLE)/2) - 5), TITLE_TEXT, font=title_font, fill="white")
    else:
        text =  "  powered by:"
        text_width = draw.textsize(text, font=font)[0]
        draw.text(((DISPLAY_SIZE[0] - text_width) / 2, font_heihgt_px), text, font=font, fill="white")
        draw.bitmap((0, 0), fs_logo, fill="white")
    
    return im


def add_scroll_bar_to_frame(im, current, total, width=1):
    draw = ImageDraw.Draw(im)
    scroll_y_start = DISPLAY_SIZE[1]/6 * 2  # Scroll is shown in lines 2-6 only
    total_height = DISPLAY_SIZE[1] - scroll_y_start
    current_y_start = scroll_y_start + current * total_height/total
    draw.rectangle((DISPLAY_SIZE[0] - width, current_y_start, DISPLAY_SIZE[0],  current_y_start + total_height/total), outline="white", fill="white")
    return im


def add_centered_value_to_frame(im, value, font_size_big=True, y_offset_lines=2):
    draw = ImageDraw.Draw(im)
    message_text = "{0}".format(value)
    font_to_use = font_big if font_size_big else font
    text_width, text_height = draw.multiline_textsize(message_text, font=font_to_use)
    y_offset = font_heihgt_px * y_offset_lines
    draw.multiline_text(((DISPLAY_SIZE[0] - text_width) / 2, y_offset + (DISPLAY_SIZE[1] - y_offset - text_height) / 2), message_text, align="center", font=font_to_use, fill="white")    
    return im


def add_global_message_to_frame(im, message_text):
    draw = ImageDraw.Draw(im)
    draw.rectangle((5, 5 + font_heihgt_px, DISPLAY_SIZE[0] - 5, DISPLAY_SIZE[1] - 5), outline="black", fill="white")
    draw.rectangle((6, 6 + font_heihgt_px, DISPLAY_SIZE[0] - 6, DISPLAY_SIZE[1] - 6), outline="white", fill="black")
    if '\n' not in message_text:    
        # single-line text
        text_width = draw.textsize(message_text, font=font)[0]
        draw.text(((DISPLAY_SIZE[0] - text_width) / 2, font_heihgt_px + int(((DISPLAY_SIZE[1] - font_heihgt_px) - FONT_SIZE)/2)), message_text, font=font, fill="white")
    else:
        # multi-line text
        text_width, text_height = draw.multiline_textsize(message_text, font=font)
        draw.multiline_text(((DISPLAY_SIZE[0] - text_width) / 2, font_heihgt_px + (DISPLAY_SIZE[1] - font_heihgt_px - text_height) / 2), message_text, align="center", font=font, fill="white")
    return im

def add_sound_waveform_and_extras_to_frame(im, 
                                           sound_data_array, 
                                           start_sample=0, 
                                           end_sample=-1, 
                                           cursor_position=0, 
                                           scale=1.0, 
                                           slices=[],
                                           start_position=None,
                                           end_position=None,
                                           loop_start_position=None,
                                           loop_end_position=None,
                                           current_time_label=None):
    draw = ImageDraw.Draw(im)
    sound_length = sound_data_array.shape[0]
    if end_sample == -1:
        end_sample = sound_length

    # Select samples to draw
    samples = []
    displayed_section_length = end_sample - start_sample
    for i in range(start_sample, end_sample, int(displayed_section_length//DISPLAY_SIZE[0])):
        samples.append(sound_data_array[i])

    waveform_height = DISPLAY_SIZE[1] - font_heihgt_px * 2
    y_offset = (font_heihgt_px * 2 + waveform_height // 2) + 1  # Center of the waveform
    
    # Draw waveform
    for i in range(0, len(samples) - 1):
        norm_sample = numpy.clip(scale * (samples[i] * 1.0 / 32768), -1.0, 1.0) * (waveform_height // 2)
        next_norm_sample = numpy.clip(scale * (samples[i + 1] * 1.0 / 32768), -1.0, 1.0) * (waveform_height // 2)
        draw.line([(i, y_offset + norm_sample), (i + 1, y_offset + next_norm_sample)], width=1, fill="white")

    # Draw slices (vertical lines on lower half of waveform)
    if slices:
        for slice_position in slices:
            if start_sample <= slice_position <= end_sample:
                slice_x = int(((slice_position - start_sample) * 1.0 / displayed_section_length) * DISPLAY_SIZE[0])
                draw.line([(slice_x, y_offset), (slice_x, y_offset + waveform_height // 2)], width=1, fill="white")

    # Draw start/end + loop start/end positions (vertical lines on higher half of waveform)
    for position, label in [
        (start_position, 'S'),
        (end_position, 'E'),
        (loop_start_position, 'SL'),
        (loop_end_position, 'EL'),
    ]:
        if position is not None:
            position_x = int(((position - start_sample) * 1.0 / displayed_section_length) * DISPLAY_SIZE[0])
            draw.line([(position_x, y_offset - waveform_height // 2), (position_x, y_offset)], width=2, fill="white")
            if 'S' in label:
                # Draw to the right of marker
                draw.text((position_x + 3, y_offset - waveform_height // 2 + 1), label, font=font, fill="white")
            else:
                # Draw to the left of marker
                draw.text((position_x - font_width_px * len(label) , y_offset - waveform_height // 2 + 1), label, font=font, fill="white")
            
    # Draw current time label
    if current_time_label is not None:
        label_width, label_height = draw.textsize(current_time_label, font=font)
        draw.text((DISPLAY_SIZE[0] - label_width - 1, DISPLAY_SIZE[1] - label_height - 1), current_time_label, font=font, fill="white")

    # Draw cursor
    if start_sample <= cursor_position <= end_sample:
        cursor_x = int(((cursor_position - start_sample) * 1.0 / displayed_section_length) * DISPLAY_SIZE[0])
        draw.line([(cursor_x, y_offset + waveform_height // 2), (cursor_x, y_offset - waveform_height // 2)], width=1, fill="white")
    
    return im


def add_midi_keyboard_and_extras_to_frame(im, cursor_position=64, assigned_notes=[], currently_selected_range=[], show_last_note_received=-1, blinking_state=False):
    draw = ImageDraw.Draw(im)

    octave_n = cursor_position // 12
    cursor_in_octave = cursor_position % 12

    keyboard_height = DISPLAY_SIZE[1] - font_heihgt_px * 3 - 2 # Save space for two lines on top and 1 line bottom
    y_offset = font_heihgt_px * 2 + 1  # Top of the keyboard

    note_positions = [0, 0.5, 1, 1.5, 2, 3, 3.5, 4, 4.5, 5, 5.5, 6]
        
    # Draw "white" keys
    for i in range(0, 12):
        note = i + 1
        is_black_key = note in [2, 4, 7, 9, 11]
        corresponding_midi_note = octave_n * 12 + i
        is_assigned = corresponding_midi_note in assigned_notes
        is_currently_selected = corresponding_midi_note in currently_selected_range
        if not is_black_key:            
            note_height = keyboard_height
            note_width = DISPLAY_SIZE[0] / 7
            x_offset = note_positions[i] * note_width
            fill = "black" if not is_assigned else "white"
            if i is cursor_in_octave or is_currently_selected:
                fill = "black" if not blinking_state else "white"
            draw.rectangle(((x_offset, y_offset), (x_offset + note_width, y_offset + note_height)), outline="white" if fill == "black" else "black", fill=fill)

    # Draw "black" keys
    for i in range(0, 12):
        note = i + 1
        is_black_key = note in [2, 4, 7, 9, 11]
        corresponding_midi_note = octave_n * 12 + i
        is_assigned = corresponding_midi_note in assigned_notes
        is_currently_selected = corresponding_midi_note in currently_selected_range
        if is_black_key:  
            note_height = keyboard_height * 0.6    
            note_width = DISPLAY_SIZE[0] / 7
            x_offset = note_positions[i] * note_width
            fill = "black" if not is_assigned else "white"
            if i is cursor_in_octave or is_currently_selected:
                fill = "black" if not blinking_state else "white"
            draw.rectangle(((x_offset, y_offset), (x_offset + note_width, y_offset + note_height)), outline="white" if fill == "black" else "black", fill=fill)

    # Draw info text
    note_names = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B']
    if show_last_note_received == -1:
        info_label = 'Sel: {} Current: {}{}'.format(len(assigned_notes), note_names[cursor_in_octave], octave_n - 2)
    else:
        info_label = 'Last received: {}{}'.format(note_names[show_last_note_received % 12], (show_last_note_received // 12) - 2)
    label_width, label_height = draw.textsize(info_label, font=font)
    draw.text((DISPLAY_SIZE[0] // 2 - label_width // 2 - 1, DISPLAY_SIZE[1] - label_height - 1), info_label, font=font, fill="white")

    return im


def add_text_input_to_frame(im, text, cursor_position=0, start_char=0, end_char=-1, y_offset_lines=2, blinking_state=False):
    draw = ImageDraw.Draw(im)

    if end_char == -1:
        end_char = len(text)

    cursor_position_in_window = cursor_position - start_char
    n_chars = end_char - start_char
    y_offset = font_heihgt_px * y_offset_lines

    for i, char_n in enumerate(range(start_char, end_char)):
        x_offset = i * ((DISPLAY_SIZE[0] // n_chars))
        if char_n == cursor_position and blinking_state:
            caret_y_offset_top = y_offset + 2 + (DISPLAY_SIZE[1] - y_offset)//2 - font_big_heihgt_px//2
            caret_y_offset_bottom = y_offset + 2 + (DISPLAY_SIZE[1] - y_offset)//2 + font_big_heihgt_px//2
            draw.rectangle(((x_offset, caret_y_offset_top), (x_offset + font_big_width_px, caret_y_offset_bottom)), fill="white")
        draw.text((x_offset, y_offset + (DISPLAY_SIZE[1] - y_offset)//2 - font_big_heihgt_px//2), text[char_n], font=font_big, fill="black" if char_n == cursor_position and blinking_state else "white")    
        
    return im 


# -- Timer for delayed actions

def delay(delay=0.):
    """
    Decorator delaying the execution of a function for a while.
    Adapted from: https://codeburst.io/javascript-like-settimeout-functionality-in-python-18c4773fa1fd
    """
    def wrap(f):
        @wraps(f)
        def delayed(*args, **kwargs):
            timer = threading.Timer(delay, f, args=args, kwargs=kwargs)
            timer.start()
        return delayed
    return wrap

class Timer():
    """
    Adapted from: https://codeburst.io/javascript-like-settimeout-functionality-in-python-18c4773fa1fd
    """
    toClearTimer = False
    
    def setTimeout(self, fn, args, time):
        isInvokationCancelled = False
    
        @delay(time)
        def some_fn():
                if (self.toClearTimer is False):
                        fn(*args)
                else:
                    # Invokation is cleared!
                    pass
    
        some_fn()
        return isInvokationCancelled
    
    def setClearTimer(self):
        self.toClearTimer = True


# -- Licenses

LICENSE_UNKNOWN = 'Unknown'
LICENSE_CC0 = 'CC0'
LICENSE_CC_BY = 'BY'
LICENSE_CC_BY_SA = 'BY-SA'
LICENSE_CC_BY_NC = 'BY-NC'
LICENSE_CC_BY_ND = 'BY-ND'
LICENSE_CC_BY_NC_SA = 'BY-NC-SA'
LICENSE_CC_BY_NC_ND = 'BY-NC-ND'
LICENSE_CC_SAMPLING_PLUS = 'SamplingPlus'

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


# -- Other

def merge_dicts(dict_a, dict_b):
    dict_merged = dict_a.copy()
    dict_merged.update(dict_b)
    return dict_merged
