import push2_python

import colorsys
import json
import os
import subprocess
import sys
import threading

from functools import wraps

from push2_python.constants import ANIMATION_STATIC

# Add specific directory to python so we can import pyshepherd
sys.path.append(os.path.join(os.path.dirname(os.path.dirname(os.path.realpath(__file__))), 'SourceSampler/3rdParty/shepherd/'))
import pyshepherd.pyshepherd

VERSION = '0.30'
CURRENT_COMMIT = str(subprocess.check_output(["git", "rev-parse", "--short", "HEAD"]).strip())[2:-1]
RUNNING_ON_RPI = os.path.exists('/sys/firmware/devicetree/base/model') and 'raspberry pi' in '\n'.join(open('/sys/firmware/devicetree/base/model').readlines()).lower()

DELAYED_ACTIONS_APPLY_TIME = 1.0  # Encoder changes won't be applied until this time has passed since last moved

LAYOUT_MELODIC = 'lmelodic'
LAYOUT_RHYTHMIC = 'lrhythmic'
LAYOUT_SLICES = 'lslices'

NOTIFICATION_TIME = 3

BLACK_RGB = [0, 0, 0]
GRAY_DARK_RGB = [30, 30, 30] if RUNNING_ON_RPI else [90, 90, 90] 
GRAY_LIGHT_RGB = [180, 180, 180]
WHITE_RGB = [255, 255, 255]
YELLOW_RGB = [255, 241, 0]
ORANGE_RGB = [255, 140, 0]
RED_RGB = [232, 17, 35]
PINK_RGB = [236, 0, 140]
PURPLE_RGB = [104, 33, 122]
BLUE_RGB = [0, 24, 143]
CYAN_RGB = [0, 188, 242]
TURQUOISE_RGB = [0, 178, 148]
GREEN_RGB = [0, 158, 73]
LIME_RGB = [186, 216, 10]

BLACK = 'black'
GRAY_DARK = 'gray_dark'
GRAY_LIGHT = 'gray_light'
WHITE = 'white'
YELLOW = 'yellow'
ORANGE = 'orange'
RED = 'red'
PINK = 'pink'
PURPLE = 'purple'
BLUE = 'blue'
CYAN = 'cyan'
TURQUOISE = 'turquoise'
GREEN = 'green'
LIME = 'lime'

COLORS_NAMES = [ORANGE, YELLOW, TURQUOISE, LIME, RED, PINK, PURPLE, BLUE, CYAN, GREEN, BLACK, GRAY_DARK, GRAY_LIGHT, WHITE]

def get_color_rgb(color_name):
    return globals().get('{0}_RGB'.format(color_name.upper()), [0, 0, 0])

def get_color_rgb_float(color_name):
    return [x/255 for x in get_color_rgb(color_name)]


# Create darker1 and darker2 versions of each color in COLOR_NAMES, add new colors back to COLOR_NAMES
to_add_in_color_names = []
for name in COLORS_NAMES:

    # Create darker 1
    color_mod = 0.35  # < 1 means make colour darker, > 1 means make colour brighter
    c = colorsys.rgb_to_hls(*get_color_rgb_float(name))
    darker_color = colorsys.hls_to_rgb(c[0], max(0, min(1, color_mod * c[1])), c[2])
    new_color_name = f'{name}_darker1'
    globals()[new_color_name.upper()] = new_color_name
    if new_color_name not in COLORS_NAMES:
        to_add_in_color_names.append(new_color_name)
    new_color_rgb_name = f'{name}_darker1_rgb'
    globals()[new_color_rgb_name.upper()] = list([c * 255 for c in darker_color])

    # Create darker 2
    color_mod = 0.05  # < 1 means make colour darker, > 1 means make colour brighter
    c = colorsys.rgb_to_hls(*get_color_rgb_float(name))
    darker_color = colorsys.hls_to_rgb(c[0], max(0, min(1, color_mod * c[1])), c[2])
    new_color_name = f'{name}_darker2'
    globals()[new_color_name.upper()] = new_color_name
    if new_color_name not in COLORS_NAMES:
        to_add_in_color_names.append(new_color_name)
    new_color_rgb_name = f'{name}_darker2_rgb'
    globals()[new_color_rgb_name.upper()] = list([c * 255 for c in darker_color])

COLORS_NAMES += to_add_in_color_names  # Update list of color names with darkified versiond of existing colors

FONT_COLOR_DELAYED_ACTIONS = ORANGE
FONT_COLOR_DISABLED = GRAY_LIGHT
OFF_BTN_COLOR = GRAY_DARK
NOTE_ON_COLOR = GREEN

DEFAULT_ANIMATION = push2_python.constants.ANIMATION_PULSING_QUARTER

USER_DOCUMENTS_PATH = os.path.expanduser('~/Documents/')
SOURCE_BASE_DATA_DIR = os.path.join(USER_DOCUMENTS_PATH, 'SourceShepherd')
os.makedirs(SOURCE_BASE_DATA_DIR, exist_ok=True)
SHEPHERD_BASE_DATA_DIR = os.path.join(USER_DOCUMENTS_PATH, 'SourceShepherd/sequencer')
os.makedirs(SHEPHERD_BASE_DATA_DIR, exist_ok=True)
SETTINGS_FILE_PATH = os.path.join(SHEPHERD_BASE_DATA_DIR, 'controllerSettings.json')
DEVICE_DEFINITION_FOLDER = os.path.join(SHEPHERD_BASE_DATA_DIR, 'device_definitions')
os.makedirs(DEVICE_DEFINITION_FOLDER, exist_ok=True)
FAVOURITE_PRESETS_FILE_PATH = os.path.join(SHEPHERD_BASE_DATA_DIR, 'favourite_presets.json')

hardwareDevicesConfigFilePath = os.path.join(SHEPHERD_BASE_DATA_DIR, 'hardwareDevices.json')
if not os.path.exists(hardwareDevicesConfigFilePath):
    hardware_devices_config = [{
		"type": "input",
		"name": "Push",
	    "shortName": "Push",
	    "midiInputDeviceName": "Push2Simulator",  # or "Ableton Push 2 Live Port", when using a real Push device
	    "controlChangeMessagesAreRelative": True,
	    "notesMapping": "-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1",
	    "controlChangeMapping": "-1,1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,64,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1"
	}]
    json.dump(hardware_devices_config, open(hardwareDevicesConfigFilePath, 'w'), indent=4)

backendSettingsConfigFilePath = os.path.join(SHEPHERD_BASE_DATA_DIR, 'backendSettings.json')
if not os.path.exists(backendSettingsConfigFilePath):
    backend_settings_config = {
        "metronomeMidiDevice": "ShpInternalOutput",
        "metronomeMidiChannel": "16",
        "midiDevicesToSendClockTo": [],
        "pushClockDeviceName": "Ableton Push 2 Live Port",  # This name should change depending on the platform where the app is running, this is for macOS
    }
    json.dump(backend_settings_config, open(backendSettingsConfigFilePath, 'w'), indent=4)

BUTTON_LONG_PRESS_TIME = 0.25
BUTTON_DOUBLE_PRESS_TIME = 0.2


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


class ShepherdControllerMode(object):
    name = ''
    xor_group = None
    buttons_used = []

    def __init__(self, app, settings=None):
        self.app = app
        self.initialize(settings=settings)

    @property
    def state(self) -> pyshepherd.pyshepherd.State:
        return self.app.state  # will be None if no session is synced

    @property
    def session(self) -> pyshepherd.pyshepherd.Session:
        return self.app.session  # will be None if no session is synced

    @property
    def push(self) -> push2_python.Push2:
        return self.app.push

    # Method run only once when the mode object is created, may receive settings dictionary from main app
    def initialize(self, settings=None):
        pass

    # Method to return a dictionary of properties to store in a settings file, and that will be passed to
    # initialize method when object created
    def get_settings_to_save(self):
        return {}

    # Methods that are run before the mode is activated and when it is deactivated
    def activate(self):
        pass

    def deactivate(self):
        # Default implementation is to set all buttons used (if any) to black
        self.set_buttons_to_color(self.buttons_used, BLACK)

    # Method called at every iteration in the main loop to see if any actions need to be performed at the end of the iteration
    # This is used to avoid some actions unncessesarily being repeated many times
    def check_for_delayed_actions(self):
        pass

    # Method called when MIDI messages arrive from ShepherdController MIDI input
    def on_midi_in(self, msg, source=None):
        pass

    # Push2 update methods
    def update_pads(self):
        pass

    def update_buttons(self):
        pass

    def update_display(self, ctx, w, h):
        pass

    # Some update helper methods
    def set_button_color(self, button_name, color=WHITE, animation=ANIMATION_STATIC, animation_end_color=BLACK):
        self.push.buttons.set_button_color(button_name, color, animation=animation, animation_end_color=animation_end_color)

    def set_button_color_if_pressed(self, button_name, color=WHITE, off_color=OFF_BTN_COLOR, animation=ANIMATION_STATIC, animation_end_color=BLACK):
        if not self.app.is_button_being_pressed(button_name):
            self.push.buttons.set_button_color(button_name, off_color)
        else:
            self.push.buttons.set_button_color(button_name, color, animation=animation, animation_end_color=animation_end_color)

    def set_button_color_if_expression(self, button_name, expression, color=WHITE, false_color=OFF_BTN_COLOR, animation=ANIMATION_STATIC, animation_end_color=BLACK, also_include_is_pressed=False):
        if also_include_is_pressed:
            expression = expression or self.app.is_button_being_pressed(button_name)
        if not expression:
            self.push.buttons.set_button_color(button_name, false_color)
        else:
            self.push.buttons.set_button_color(button_name, color, animation=animation, animation_end_color=animation_end_color)

    def set_buttons_to_color(self, button_names, color=WHITE, animation=ANIMATION_STATIC, animation_end_color=BLACK):
        for button_name in button_names:
            self.push.buttons.set_button_color(button_name, color, animation=animation, animation_end_color=animation_end_color)

    def set_buttons_need_update_if_button_used(self, button_name):
        if button_name in self.buttons_used:
            self.app.buttons_need_update = True

    # Push2 action callbacks (these methods should return True if some action was carried out, otherwise return None)
    def on_encoder_rotated(self, encoder_name, increment):
        pass

    def on_button_pressed_raw(self, button_name):
        pass

    def on_button_released_raw(self, button_name):
        pass

    def on_pad_pressed_raw(self, pad_n, pad_ij, velocity):
        pass

    def on_pad_released_raw(self, pad_n, pad_ij, velocity):
        pass

    def on_pad_aftertouch(self, pad_n, pad_ij, velocity):
        pass

    def on_touchstrip(self, value):
        pass

    def on_sustain_pedal(self, sustain_on):
        pass

    # Processed Push2 action callbacks that allow to easily diferentiate between actions like "button single press", "button double press", "button long press", "button single press + shift"...
    def on_button_pressed(self, button_name, shift=False, select=False, long_press=False, double_press=False):
        pass

    def on_pad_pressed(self, pad_n, pad_ij, velocity, shift=False, select=False, long_press=False, double_press=False):
        pass
