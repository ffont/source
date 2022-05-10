import asyncio
import datetime
import json
import math
import sys

import numpy
import os
import time
import threading
import fnmatch
import urllib

from collections import deque
from functools import wraps
from PIL import ImageFont, Image, ImageDraw

# -- Various

ALLOWED_AUDIO_FILE_EXTENSIONS = ['ogg', 'wav', 'aiff', 'mp3', 'flac']


def get_platform():
    if sys.platform == "linux" or sys.platform == "linux2":
        platform = "ELK"
    else:
        platform = "desktop"
    return platform


def get_command_output(command):
    return os.popen(command).read()[:-1]  # Remove last char which is always \n


# -- Porcessed state names

class PlStateNames():

    SYSTEM_STATS = 'SYSTEM_STATS'
    CONNECTION_WITH_PLUGIN_OK = 'CONNECTION_WITH_PLUGIN_OK'
    NETWORK_IS_CONNECTED = 'NETWORK_IS_CONNECTED'

    SOURCE_DATA_LOCATION = 'sourceDataLocation'
    SOUNDS_DATA_LOCATION = 'soundsDownloadLocation'
    PRESETS_DATA_LOCATION = 'presetFilesLocation'
    TMP_DATA_LOCATION = 'tmpFilesLocation'

    PLUGIN_VERSION = 'pluginVersion'

    USE_ORIGINAL_FILES_PREFERENCE = 'useOriginalFilesPreference'
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


# -- Sound parameters helpers

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
    "pan": (lambda x: 2.0 * (snap_to_value(x) - 0.5), lambda x: float(x), "Panning", "{0:.1f}", "/set_sound_parameter"),
    "loopXFadeNSamples": (lambda x: 10 + int(round(lin_to_exp(x) * (100000 - 10))), lambda x: int(x), "Loop X fade len", "{0}", "/set_sound_parameter_int"),  
    "midiChannel": (lambda x: int(round(16 * x)), lambda x: ['Global', "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16"][int(x)], "MIDI channel", "{0}", "/set_sound_parameter_int"),
}


# -- UI helpers

DISPLAY_SIZE = (128, 64)
FONT_PATH = 'resources/LiberationMono-Regular.ttf'
FONT_SIZE = 10
FONT_SIZE_BIG = 25
FONT_PATH_TITLE = 'resources/FuturaHeavyfont.ttf'
FONT_SIZE_TITLE = 64 - 10
RA_LOGO_PATH = 'resources/logo_oled_ra.png'
RA_LOGO_B_PATH = 'resources/logo_oled_ra_b.png'
FS_LOGO_PATH = 'resources/logo_oled_fs.png'
UPF_LOGO_PATH = 'resources/logo_oled_upf.png'
#TITLE_TEXT = '                             SOURCE, by Rita & Aurora
TITLE_TEXT = '                                                 SOURCE                            ' 
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
upf_logo = Image.open(UPF_LOGO_PATH).resize(DISPLAY_SIZE, Image.NEAREST).convert('1')

moving_text_position_cache = {}


def clear_moving_text_cache():
    global moving_text_position_cache
    moving_text_position_cache = {}


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
            move = line.get('move', False) and len(line['text']) >= DISPLAY_SIZE[0] // font_width_px  # If text is long, keep on moving it
            text_color = "white"
            if invert_colors:
                draw.rectangle((0, y_offset, DISPLAY_SIZE[0], y_offset + font_heihgt_px), outline="white", fill="white")
                text_color = "black"
            if underline:
                draw.line((0, y_offset + font_heihgt_px, DISPLAY_SIZE[0], y_offset + font_heihgt_px), fill="white")
            if move:
                line['text'] += '   '
                if line['text'] not in moving_text_position_cache:
                    moving_text_position_cache[line['text']] = (0, time.time())
                else:
                    last_time_moved = moving_text_position_cache[line['text']][1]
                    n_moved = moving_text_position_cache[line['text']][0]
                    if time.time() - last_time_moved > (2.5 if n_moved % len(line['text']) == 0 else 0.15):
                        moving_text_position_cache[line['text']] = (n_moved + 1, time.time())
                    
                n_to_move = moving_text_position_cache[line['text']][0] % len(line['text'])
                dequeued_text = deque([c for c in line['text']])
                dequeued_text.rotate(-1 * n_to_move)
                text = ''.join(list(dequeued_text))
            else:
                text = line['text']

            draw.text( (1, y_offset), text, font=font, fill=text_color)

        y_offset += font_heihgt_px + 1     

    return im


def frame_from_start_animation(progress, counter):
    global title_text_width

    im = Image.new(mode='1', size=DISPLAY_SIZE)
    draw = ImageDraw.Draw(im)
    if progress < 0.15:
        # Show UPF logo
        draw.bitmap((0, 0), upf_logo, fill="white")
    elif 0.15 <= progress <= 0.45:
        # Show R&A logo
        if counter % 2 == 0:
            draw.bitmap((0, 0), ra_logo, fill="white")
        else:
            draw.bitmap((0, 0), ra_logo_b, fill="white")
    elif 0.45 <= progress <= 0.75:
        # Show title text
        if title_text_width is None:
            title_text_width = draw.textsize(TITLE_TEXT, font=title_font)[0]
        draw.text((int(progress * -1 * title_text_width), int((DISPLAY_SIZE[1] - FONT_SIZE_TITLE)/2) - 5), TITLE_TEXT, font=title_font, fill="white")
    else:
        # Show Freesound logo
        text =  "  powered by:"
        text_width = draw.textsize(text, font=font)[0]
        draw.text(((DISPLAY_SIZE[0] - text_width) / 2, font_heihgt_px), text, font=font, fill="white")
        draw.bitmap((0, 0), fs_logo, fill="white")
    
    return im


def add_scroll_bar_to_frame(im, current, total, width=1):
    draw = ImageDraw.Draw(im)
    scroll_y_start = DISPLAY_SIZE[1]/6 * 2  # Scroll is shown in lines 2-6 only
    total_height = DISPLAY_SIZE[1] - scroll_y_start
    current_y_start = scroll_y_start + current * total_height/(total + 1)
    draw.rectangle((DISPLAY_SIZE[0] - width, current_y_start, DISPLAY_SIZE[0],  current_y_start + total_height/(total + 1)), outline="white", fill="white")
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
                                           current_time_label=None,
                                           blinking_state=False):
    draw = ImageDraw.Draw(im)
    sound_length = sound_data_array.shape[0]
    if end_sample == -1:
        end_sample = sound_length

    # Select samples to draw
    displayed_section_length = end_sample - start_sample
    samples_per_pixel = int(displayed_section_length//DISPLAY_SIZE[0])
    max_samples_to_include_on_pixel_average_calculation = min(samples_per_pixel, 10000)  # This is to avoid too much computational cost when samples_per_pixel is very high
    samples = []
    for i in range(start_sample, end_sample, samples_per_pixel):
        # Compute mean of the positive values of the audio chunk and also of the negative values
        # Normalize by maximum value number, multiply by scale and clip to -1, 1
        audio_chunk = sound_data_array[i:i+max_samples_to_include_on_pixel_average_calculation]
        positive_samples = audio_chunk[audio_chunk >= 0]
        if len(positive_samples) > 0:
            avg_pos = positive_samples.mean()
            avg_pos = numpy.clip(scale * avg_pos, -1.0, 1.0)
        else:
            avg_pos = None   
        negative_samples = audio_chunk[audio_chunk <= 0]
        if len(negative_samples) > 0:
            avg_neg = negative_samples.mean()
            avg_neg = numpy.clip(scale * avg_neg, -1.0, 1.0)
        else:
            avg_neg = None     
        samples.append((avg_pos, avg_neg))

    # Draw waveform
    waveform_height = DISPLAY_SIZE[1] - font_heihgt_px * 2
    y_offset = (font_heihgt_px * 2 + waveform_height // 2) + 1  # Center of the waveform
    for i, (avg_pos, avg_neg) in enumerate(samples):

        def get_non_none(val1, val2):
            if val1 is not None:
                return val1
            elif val2 is not None:
                return val2
            else:
                return 0.0

        if avg_neg is None or avg_pos is None:
            # The zoom is high enough so that the samples per pixel only contains samples on negative or positive side
            # In that case, we draw a line from the current pixel to the next
            current_pixel_val = get_non_none(avg_pos, avg_neg)
            if i <= len(samples) - 2:
                next_pixel_val = get_non_none(*samples[i + 1])
            else:
                next_pixel_val = 0
            draw.line([(i, y_offset + current_pixel_val * waveform_height // 2), (i + 1, y_offset + next_pixel_val * waveform_height // 2)], width=1, fill="white")
            
        else:
            # We have a positive value of the average ans also a negative value
            # We draw a line from the positive to the negative
            draw.line([(i, y_offset + avg_pos * waveform_height // 2), (i, y_offset + avg_neg * waveform_height // 2)], width=1, fill="white")


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
            
    # Draw cursor
    if start_sample <= cursor_position <= end_sample:
        cursor_x = int(((cursor_position - start_sample) * 1.0 / displayed_section_length) * DISPLAY_SIZE[0])
        draw.line([(cursor_x, y_offset + waveform_height // 2), (cursor_x, y_offset - waveform_height // 2 + 1)], width=1, fill="white" if blinking_state else "black")
    
    # Draw current time
    if current_time_label is not None:
        label_width, label_height = draw.textsize(current_time_label, font=font)
        draw.text((DISPLAY_SIZE[0] - label_width - 1, DISPLAY_SIZE[1] - label_height - 1), current_time_label, font=font, fill="white")
    
    # Draw scale label
    scale_label = 'x{:.1f}'.format(scale)
    label_width, label_height = draw.textsize(scale_label, font=font)
    draw.text((2, DISPLAY_SIZE[1] - label_height - 1), scale_label, font=font, fill="white")
    
    return im


def add_midi_keyboard_and_extras_to_frame(im, cursor_position=64, assigned_notes=[], currently_selected_range=[], last_note_received=-1, blinking_state=False, root_note=None):
    draw = ImageDraw.Draw(im)

    octave_n = cursor_position // 12
    cursor_in_octave = cursor_position % 12

    keyboard_height = DISPLAY_SIZE[1] - font_heihgt_px * 3 - 2 # Save space for two lines on top and 1 line bottom
    y_offset = font_heihgt_px * 2 + 1  # Top of the keyboard

    selected_inset_margin = 3

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
            draw.rectangle(((x_offset, y_offset), (x_offset + note_width, y_offset + note_height)), outline="white" if fill == "black" else "black", fill=fill)
            if i is cursor_in_octave or is_currently_selected:
                outline = "black" if not blinking_state else "white"
                draw.rectangle(((x_offset + selected_inset_margin, y_offset + selected_inset_margin), (x_offset + note_width - selected_inset_margin, y_offset + note_height - selected_inset_margin)), outline=outline, fill=fill)
            if corresponding_midi_note == root_note  or corresponding_midi_note == last_note_received:
                label = "*" if corresponding_midi_note == last_note_received else "R"
                fill = "black" if fill == "white" else "white"
                draw.text((x_offset + note_width // 2 - font_width_px // 2 + 1, y_offset + note_height - font_heihgt_px - 2), label, font=font, fill=fill)

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
            draw.rectangle(((x_offset, y_offset), (x_offset + note_width, y_offset + note_height)), outline="white" if fill == "black" else "black", fill=fill)
            if i is cursor_in_octave or is_currently_selected:
                outline = "black" if not blinking_state else "white"
                draw.rectangle(((x_offset + selected_inset_margin, y_offset + selected_inset_margin), (x_offset + note_width - selected_inset_margin, y_offset + note_height - selected_inset_margin)), outline=outline, fill=fill)
            if corresponding_midi_note == root_note  or corresponding_midi_note == last_note_received:
                label = "*" if corresponding_midi_note == last_note_received else "R"
                fill = "black" if fill == "white" else "white"
                draw.text((x_offset + note_width // 2 - font_width_px // 2 + 1, y_offset + note_height - font_heihgt_px - 4), label, font=font, fill=fill)

    # Draw info text
    note_names = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B']
    if last_note_received == -1:
        info_label = 'N:{} Cur:{}{} Root:{}{}'.format(len(assigned_notes), note_names[cursor_in_octave], octave_n - 2, note_names[root_note % 12], root_note // 12 - 2)
    else:
        info_label = 'Last received: {}{}'.format(note_names[last_note_received % 12], (last_note_received // 12) - 2)
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


def add_meter_to_frame(im, y_offset_lines=3, value=0.5, margin_x_left=15, margin_x_right=-1):
    draw = ImageDraw.Draw(im)
    width = DISPLAY_SIZE[0] - margin_x_left - margin_x_right
    draw.rectangle((margin_x_left, 1 + (font_heihgt_px) * y_offset_lines, margin_x_left + width, 2 + (font_heihgt_px) * (y_offset_lines + 1)), outline="white", fill="black")
    draw.rectangle((margin_x_left, 1 +(font_heihgt_px) * y_offset_lines, margin_x_left + width * value, 2 + (font_heihgt_px) * (y_offset_lines + 1)), outline="white", fill="white")

    value_in_db = 20 * numpy.log10(max(0.000001, value))
    label = '{:.1f}dB'.format(value_in_db) if value_in_db > -100 else ''
    label_width, _ = draw.textsize(label, font=font)
    draw.text((margin_x_left + width - label_width -2, 1 + (font_heihgt_px) * y_offset_lines), label, font=font, fill="white")
           
    return im 


def add_voice_grid_to_frame(im, y_offset_lines=4, voice_activations=[], max_columns=8):
    draw = ImageDraw.Draw(im)
    voice_activations = voice_activations[:16]  # Make sure we don't have more than 16 (so we wont' have more than 2 rows)
    n_in_row = min(len(voice_activations), max_columns) 
    n_rows = int(math.ceil(len(voice_activations) / n_in_row))
    item_width =  DISPLAY_SIZE[0] // max_columns - 1
    item_height = font_heihgt_px
    total_height = item_height * n_rows
    y_offset_grid = y_offset_lines * (font_heihgt_px)
    if voice_activations:
        for i in range(0, n_rows):
            y_offset = y_offset_grid + (DISPLAY_SIZE[1] - y_offset_grid - total_height) // 2 + i * item_height + 1
            activations_in_row = voice_activations[i * n_in_row:(i + 1) * n_in_row] 
            margin_x = (DISPLAY_SIZE[0] - len(activations_in_row) * item_width) // 2
            for j, activation in enumerate(activations_in_row):
                x_offset = margin_x + j * item_width
                draw.rectangle((x_offset, y_offset, x_offset + item_width, y_offset + item_height), outline="white", fill="black")
                if activation > -1:
                    draw.rectangle((x_offset, y_offset, x_offset + item_width, y_offset + item_height), outline="white", fill="white")
                    width, _ = draw.textsize(str(activation), font=font)
                    draw.text((x_offset + (item_width - width) // 2, y_offset), str(activation + 1), font=font, fill="black")

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


# -- Sound usage log

class SoundUsageLogManager(object):

    sound_usage_log = {}
    sound_usage_log_base_dir = ''
    today_sound_usage_log_path = ''

    def __init__(self, data_dir):
        self.sound_usage_log_base_dir = os.path.join(data_dir, 'sound_usage_log')
        if not os.path.exists(self.sound_usage_log_base_dir):
            os.makedirs(self.sound_usage_log_base_dir)
        
        now = datetime.datetime.now()
        self.today_sound_usage_log_path = os.path.join(self.sound_usage_log_base_dir, '{}_{:02d}_{:02d}_sound_usage.json'.format(now.year, now.month, now.day))
        if os.path.exists(self.today_sound_usage_log_path):
            self.sound_usage_log = json.load(open(self.today_sound_usage_log_path, 'r'))

    def log_sound(self, sound):
        sound_id = sound.get(PlStateNames.SOUND_ID.lower(), None)
        if sound_id is not None and sound_id not in self.sound_usage_log:
            self.sound_usage_log[sound_id] = {
                'name': sound.get("name", '-'),
                'url': 'https://freesound.org/s/{}'.format(sound_id),
                'username': sound.get(PlStateNames.SOUND_AUTHOR.lower(), '-'),
                'license': sound.get(PlStateNames.SOUND_LICENSE.lower(), '-'),
                'id': sound_id,
                'time_loaded': str(datetime.datetime.today()) 
            }
            json.dump(self.sound_usage_log, open(self.today_sound_usage_log_path, 'w'), indent=4)

sound_usage_log_manager = None

def log_sound_used(sound):
    if sound_usage_log_manager is not None:
        sound_usage_log_manager.log_sound(sound)

def get_all_sound_usage_logs():
    log = []
    if sound_usage_log_manager is not None:
        for filename in os.listdir(sound_usage_log_manager.sound_usage_log_base_dir):
            if '_sound_usage' in filename:
                log.append((
                    filename.split('_sound_usage')[0].replace('_', '-'),
                    sorted(json.load(open(os.path.join(sound_usage_log_manager.sound_usage_log_base_dir, filename), 'r')).values(), key=lambda x: x['time_loaded'], reverse=True)
                ))
    log = sorted(log, key=lambda x:x[0], reverse=True)
    return log

# -- Other

def merge_dicts(dict_a, dict_b):
    dict_merged = dict_a.copy()
    dict_merged.update(dict_b)
    return dict_merged


def raw_assigned_notes_to_midi_assigned_notes(raw_assigned_notes):
    # raw_assigned_notes is  a hex string representation of the JUCE BigInteger for assigned midi notes
    # This returns a list with the MIDI note numbers corresponding to the hex string
    bits_raw = [bit == '1' for bit in "{0:b}".format(int(raw_assigned_notes, 16))]
    bits = [False] * (128 - len(bits_raw)) + bits_raw
    return [i for i, bit in enumerate(reversed(bits)) if bit]


def get_filenames_in_dir(dir_name, keyword='*', skip_foldername='', match_case=True, verbose=False):
    """TODO: better document this function
    # FROM PYMTG
    TODO: does a python 3 version of this function exist?

    Args:
        dir_name (str): The foldername.
        keyword (str): The keyword to search (defaults to '*').
        skip_foldername (str): An optional foldername to skip searching
        match_case (bool): Flag for case matching
        verbose (bool): Verbosity flag

    Returns:
        (tuple): Tuple containing:
            - fullnames (list): List of the fullpaths of the files found
            - folder (list): List of the folders of the files
            - names (list): List of the filenames without the foldername

    Examples:
        >>> get_filenames_in_dir('/path/to/dir/', '*.mp3')  #doctest: +SKIP
        (['/path/to/dir/file1.mp3', '/path/to/dir/folder1/file2.mp3'], ['/path/to/dir/', '/path/to/dir/folder1'], ['file1.mp3', 'file2.mp3'])
    """
    names = []
    folders = []
    fullnames = []

    if verbose:
        print(dir_name)

    # check if the folder exists
    if not os.path.isdir(dir_name):
        if verbose:
            print("Directory doesn't exist!")
        return [], [], []

    # if the dir_name finishes with the file separator,
    # remove it so os.walk works properly
    dir_name = dir_name[:-1] if dir_name[-1] == os.sep else dir_name

    # walk all the subdirectories
    for (path, dirs, files) in os.walk(dir_name):
        for f in files:
            hasKey = (fnmatch.fnmatch(f, keyword) if match_case else
                      fnmatch.fnmatch(f.lower(), keyword.lower()))
            if hasKey and skip_foldername not in path.split(os.sep)[1:]:
                folders.append(path)
                names.append(path)
                fullnames.append(os.path.join(path, f))

    if verbose:
        print("> Found " + str(len(names)) + " files.")
    return fullnames, folders, names


def sizeof_fmt(num, suffix='B'):
    '''Get human readable version of file size
    From: https://stackoverflow.com/questions/1094841/get-human-readable-version-of-file-size
    '''
    num = int(num)
    for unit in ['','Ki','Mi','Gi','Ti','Pi','Ei','Zi']:
        if abs(num) < 1024.0:
            return "%3.1f%s%s" % (num, unit, suffix)
        num /= 1024.0
    return "%.1f%s%s" % (num, 'Yi', suffix)


# -- Recent queries and query filters

class RecetQueriesAndQueryFiltersManager(object):

    recent_queries_and_filters = {
        'queries': [],
        'query_filters': [],
    }
    recent_queries_and_filters_path = ''
    n_recent_items_to_store = 10

    def __init__(self, data_dir):
        self.recent_queries_and_filters_path = os.path.join(data_dir, 'recent_queries_and_filters.json')
        if os.path.exists(self.recent_queries_and_filters_path):
            self.recent_queries_and_filters = json.load(open(self.recent_queries_and_filters_path, 'r'))
        
    def add_recent_query(self, query):
        # If already present in list, remove it. We'll re-add it later
        self.recent_queries_and_filters['queries'] = [item for item in self.recent_queries_and_filters['queries'] if item != query]
        self.recent_queries_and_filters['queries'].append(query)
        if len(self.recent_queries_and_filters['queries']) > self.n_recent_items_to_store:
            self.recent_queries_and_filters['queries'] = self.recent_queries_and_filters['queries'][len(self.recent_queries_and_filters['queries']) - self.n_recent_items_to_store:]
        json.dump(self.recent_queries_and_filters, open(self.recent_queries_and_filters_path, 'w'))

    def add_recent_query_filter(self, query_filter):
        # If already present in list, remove it. We'll re-add it later
        self.recent_queries_and_filters['query_filters'] = [item for item in self.recent_queries_and_filters['query_filters'] if json.dumps(item) != json.dumps(query_filter)]
        self.recent_queries_and_filters['query_filters'].append(query_filter)
        if len(self.recent_queries_and_filters['query_filters']) > self.n_recent_items_to_store:
            self.recent_queries_and_filters['query_filters'] = self.recent_queries_and_filters['query_filters'][len(self.recent_queries_and_filters['query_filters']) - self.n_recent_items_to_store:]
        json.dump(self.recent_queries_and_filters, open(self.recent_queries_and_filters_path, 'w'))

    def get_recent_queries(self):
        return list(reversed(self.recent_queries_and_filters['queries']))

    def get_recent_query_filters(self):
        return list(reversed(self.recent_queries_and_filters['query_filters']))


recent_queries_and_filters = None

def add_recent_query(query):
    if recent_queries_and_filters is not None:
        recent_queries_and_filters.add_recent_query(query) 

def add_recent_query_filter(query_filter):
    if recent_queries_and_filters is not None:
        recent_queries_and_filters.add_recent_query_filter(query_filter) 


def get_recent_queries():
    if recent_queries_and_filters is not None:
        return recent_queries_and_filters.get_recent_queries()
    else:
        return []
    
def get_recent_query_filters():
    if recent_queries_and_filters is not None:
        return recent_queries_and_filters.get_recent_query_filters()
    else:
        return []


tmp_base_path = None
# File downloader for general purpose threaded downloads
class DownloadFileThread(threading.Thread):

    url = None
    outfile = None

    def __init__(self, url, outfile):
        super(DownloadFileThread, self).__init__()
        self.url = url
        self.outfile = outfile

    def run(self):
        asyncio.set_event_loop(asyncio.new_event_loop())        
        if tmp_base_path is not None:
            outfile = os.path.join(tmp_base_path, self.outfile)
        
            if not (os.path.exists(outfile) and os.path.getsize(outfile) > 0):
                # If sound does not exist, start downloading
                import sys
                print('- Downloading tmp file: ' + self.url + ' to ' + outfile)
                sys.stdout.flush()
                urllib.request.urlretrieve(self.url, outfile)


def configure_recent_queries_sound_usage_log_tmp_base_path_from_source_state(source_data_location, tmp_data_location):
    global recent_queries_and_filters, sound_usage_log_manager, tmp_base_path

     # When full state is received, setup some objects
    if recent_queries_and_filters is None:
        recent_queries_and_filters = RecetQueriesAndQueryFiltersManager(source_data_location)
    if sound_usage_log_manager is None:
        sound_usage_log_manager = SoundUsageLogManager(source_data_location)
    if tmp_base_path is None:
        tmp_base_path = tmp_data_location