import threading

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
    
    NUM_SOUNDS = auto()
    SOUNDS_INFO = auto()
    SOUND_NAME = auto()
    SOUND_ID = auto()
    SOUND_LICENSE = auto()
    SOUND_AUTHOR = auto()
    SOUND_DURATION = auto()
    SOUND_DOWNLOAD_PROGRESS = auto()
    SOUND_PARAMETERS = auto()

    NUM_ACTIVE_VOICES = auto()
    

def process_xml_state_from_plugin(plugin_state_xml):
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
    
    # Loaded sounds properties
    sounds_info = preset_state.find_all("soundsInfo".lower())[0].find_all("soundInfo".lower())
    source_state[StateNames.NUM_SOUNDS] = len(sounds_info)
    processed_sounds_info = []
    for sound_info in sounds_info:

        processed_sound_parameters_info = {}
        for parameter in sound_info.find_all('samplersoundparameter'):
            val = parameter['parameter_value']
            name = parameter['parameter_name']
            processed_sound_parameters_info[name] = val

        processed_sounds_info.append({
            StateNames.SOUND_NAME: sound_info.get('soundname', '-'),
            StateNames.SOUND_ID: sound_info.get('soundid', 0),
            StateNames.SOUND_LICENSE: translate_cc_license_url(sound_info.get('soundlicense', '-')),
            StateNames.SOUND_AUTHOR: sound_info.get('sounduser', '-'),
            StateNames.SOUND_DURATION: float(sound_info.get('sounddurationinseconds', 0)),
            StateNames.SOUND_DOWNLOAD_PROGRESS: '{0}'.format(int(sound_info.get('downloadprogress', 0))),
            StateNames.SOUND_PARAMETERS: processed_sound_parameters_info
        })

    source_state[StateNames.SOUNDS_INFO] = processed_sounds_info

    # Number of active voices
    source_state[StateNames.NUM_ACTIVE_VOICES] = sum([int(element) for element in volatile_state.get('voiceActivations'.lower(), '').split(',') if element])

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


def add_centered_value_to_frame(im, value, font_size_big=True):
    draw = ImageDraw.Draw(im)
    message_text = "{0}".format(value)
    text_width = draw.textsize(message_text, font=font_big if font_size_big else font)[0]
    y_offset = font_heihgt_px * 2 + 2
    height = DISPLAY_SIZE[1] - y_offset
    if font_size_big:
        draw.text(((DISPLAY_SIZE[0] - text_width) / 2, y_offset + (height - FONT_SIZE_BIG)/2), message_text, font=font_big, fill="white")
    else:
        draw.text(((DISPLAY_SIZE[0] - text_width) / 2, y_offset + (height - FONT_SIZE)/2), message_text, font=font, fill="white")
    return im


def add_global_message_to_frame(im, message_text):
    draw = ImageDraw.Draw(im)
    draw.rectangle((5, 5 + font_heihgt_px, DISPLAY_SIZE[0] - 5, DISPLAY_SIZE[1] - 5), outline="black", fill="white")
    draw.rectangle((6, 6 + font_heihgt_px, DISPLAY_SIZE[0] - 6, DISPLAY_SIZE[1] - 6), outline="white", fill="black")
    text_width = draw.textsize(message_text, font=font)[0]
    draw.text(((DISPLAY_SIZE[0] - text_width) / 2, font_heihgt_px + int(((DISPLAY_SIZE[1] - font_heihgt_px) - FONT_SIZE)/2)), message_text, font=font, fill="white")
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
