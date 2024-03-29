""" elk_ui.py

    Utility class to handle the UI on Elk Pi hats in a Pythonic way.
"""

__copyright__   = "Copyright (C) 2020 Modern Ancient Instruments Networked AB, dba Elk"
__license__ = "GPL-3.0"

import math
import time
import os.path
import liblo

from demo_opts import get_device as get_display_device
import luma.core.render as render

######################
#  Module Constants  #
######################

# Control config
N_FADERS = 4
N_BUTTONS = 9
N_LEDS = 9
LED_BASE_IDX = 1
LED_FADER_IDXS = [10, 12, 11, None]
ANALOG_SENSORS_MIN_ABS_DIFF = (1.0 / 128)
ROT_ENC_ID = 23

# OSC ports
SENSEI_TO_BRIDGE_PORT = 23023
BRIDGE_TO_SENSEI_PORT = 23024

# OSC paths
FADER_PATH_PREFIX = "/sensors/analog/fader_"
BUTTON_PATH_PREFIX = "/sensors/digital/button_"
ROT_ENC_BUTTON_PATH = "/sensors/digital/rot_enc_button"
ROT_ENC_PATH = "/sensors/analog/rot_enc"
POTENTIOMETER_PATH = "/sensors/analog/pot_A"

# OLED display
DISPLAY_CONFIG = ["--display", "ssd1306",  "--i2c-port", "0", "--i2c-address", "0x3C", "--width", "128", "--height", "64"]
RESET_PIN_INDEX = 31


####################
#  Public classes  #
####################

class ElkUIController(object):
    """ Utility class to access controls and LEDs / displays on the UI hat for Elk Pi.

        Inputs are handled through callbacks passed at initialization time.

        Outputs (LEDs & OLED display) can be controlled via the set_* methods.

        The refresh() method needs to be called periodically (e.g., every 20-30 ms)
        to refresh the OLED display.
    """

    def __init__(self,
                 faders_callback,
                 buttons_callback,
                 encoder_button_callback,
                 encoder_callback,
                 pot_callback,
                 sensei_ctrl_port=SENSEI_TO_BRIDGE_PORT,
                 sensei_led_port=BRIDGE_TO_SENSEI_PORT):
        """ Initialization.

            Inputs:
                faders_callback : function with args (fader_idx, val)
                                  Will be invoked when an analog fader is moved.
                                  Callback arguments:
                                      fader_idx : int  (0 = A, 1 = B, 2 = C, 3 = D)
                                      val     : fader value from 0.0 to 1.0

                buttons_callback : function with args (button_idx, val)
                                   It will be invoked whenever a button is pressed or released.
                                   Callback arguments:
                                        button_idx : index of the button (0..8)
                                        val        : button status (1=pressed, 0=released)

                encoder_button_callback : callback to invoke when rotary encoder button is pressed
                                          Callback arguments:
                                            val        : button status (1=pressed, 0=released)


                encoder_callback : function with a single argument (direction)
                                   Will be invoked when the encoder is rotated.
                                   Callback arguments:
                                        direction : +1 if turned right, -1 if turned left

                sensei_ctrl_port : UDP port for incoming OSC messages from Sensei
                
                sensei_led_port  : UDP port to output LED changes to Sensei
        """
        self._osc_server = liblo.ServerThread(sensei_ctrl_port)
        self._sensei_address = ('localhost', sensei_led_port)

        self._btn_cback = buttons_callback
        self._enc_btn_cback = encoder_button_callback
        self._enc_cback = encoder_callback
        self._faders_cback = faders_callback
        self._pot_cback= pot_callback
        self._register_osc_callbacks()
       
        self.reset_display()
        self._display_dev = get_display_device(DISPLAY_CONFIG)
        self._display_dirty = False

        self.set_rot_enc_initial_val()

    def set_rot_enc_initial_val(self):
        """
        Needed so that the rotary encoder is properly initialized and it can be rutned counter-clockwise from a start
        https://forum.elk.audio/t/blackboard-turning-rotary-encoder-has-no-effect-if-i-dont-turn-it-right-first/565/9
        """
        osc_msg = liblo.Message('/set_output')
        osc_msg.add(('i', ROT_ENC_ID))
        osc_msg.add(('f', 0.5))
        liblo.send(self._sensei_address, osc_msg)

    def set_led(self, idx, val):
        """ Immediately set one of the LEDs on the board.
            Inputs:
                idx : LED idx from 0 to 8
                val : 1 = LED on, 0 = LED off
        """
        osc_msg = liblo.Message('/set_output')
        osc_msg.add(('i', LED_BASE_IDX+idx))
        osc_msg.add(('f', val))
        liblo.send(self._sensei_address, osc_msg)

    def set_fader_led(self, idx, val):
        """ Immediately set one of the LEDs on the faders.
            Inputs:
                idx : fader idx from 0 to 3
                val : 1 = LED on, 0 = LED off
        """
        led_idx = LED_FADER_IDXS[idx]
        if led_idx is not None:
            osc_msg = liblo.Message('/set_output')
            osc_msg.add(('i', led_idx))
            osc_msg.add(('f', val))
            liblo.send(self._sensei_address, osc_msg)

    def set_display_frame(self, im):
        """ Sets the frame contents to display.
        'im' is a 1-bit PIL.Image object of size 128x64.
        """
        self._display_frame = im
        self._display_dirty = True

    def run(self):
        """ Starts the Sensei OSC server.
            Needs to be called once the registered callbacks are ready to receive data.
        """
        # Reset LED status
        for n in range(N_LEDS):
            self.set_led(n, 0)

        for n in range(N_FADERS):
            self.set_fader_led(n, 0)

        self._fader_values = [0.0] * N_FADERS
        self._pot_value = 0.7  # TODO: Get current pot value here
        self._enc_value = None
        self._osc_server.start()

    def refresh(self):
        """ Needs to be called periodically from the event loop to refresh the OLED display.
        """
        if not self._display_dirty:
            return

        with render.canvas(self._display_dev) as draw:

            if self._display_frame is not None:
                # If _display_frame is set, display that frame
                draw.bitmap((0, 0), self._display_frame, fill="white")

            self._display_dirty = False


    def _register_osc_callbacks(self):
        for n in range(N_FADERS):
            self._osc_server.add_method(FADER_PATH_PREFIX + str(n), 'f', self._handle_faders)
            
        for n in range(N_BUTTONS):
            self._osc_server.add_method(BUTTON_PATH_PREFIX + str(n), 'f', self._handle_buttons)

        self._osc_server.add_method(ROT_ENC_PATH, 'f', self._handle_encoder)
        self._osc_server.add_method(ROT_ENC_BUTTON_PATH, 'f', self._handle_encoder_button)

        self._osc_server.add_method(POTENTIOMETER_PATH, 'f', self._handle_potentiometer)

        self._osc_server.add_method(None, None, self._unhandled_msg_callback)

    def _unhandled_msg_callback(self, path, args, types, src):
        print('Unknown OSC message %s from %s' % (path, src.url))

    def _handle_faders(self, path, args):
        if self._faders_cback is None:
            return

        idx = int(path.split('_')[-1])
        val = args[0]
        # Extra filtering for noise in faders
        if (abs(self._fader_values[idx] - val) > ANALOG_SENSORS_MIN_ABS_DIFF):
            self._faders_cback(idx, val)
            self._fader_values[idx] = val

    def _handle_buttons(self, path, args):
        if self._btn_cback is None:
            return

        idx = int(path.split('_')[-1])
        val = args[0]
        self._btn_cback(idx, int(val))

    def _handle_encoder_button(self, path, args):
        if self._enc_btn_cback is None:
            return
        val = args[0]
        self._enc_btn_cback(int(val))

    def _handle_encoder(self, path, args):
        if self._enc_cback is None:
            return

        val = args[0]
        if self._enc_value is None:
            self._enc_value = val
        else:
            val_diff = val - self._enc_value
            if (abs(val_diff) > 0):
                direction = int(math.copysign(1, val_diff))
                self._enc_cback(direction)
                self._enc_value = val

    def _handle_potentiometer(self, path, args):
        if self._pot_cback is None:
            return

        val = args[0]
        # Extra filtering for noise in faders
        if (abs(self._pot_value - val) > ANALOG_SENSORS_MIN_ABS_DIFF):
            self._pot_cback(val)
            self._pot_value = val
        
    def reset_display(self):
        osc_msg = liblo.Message('/set_output')
        osc_msg.add(('i', RESET_PIN_INDEX))
        osc_msg.add(('i', 0))

        liblo.send(self._sensei_address, osc_msg)
        time.sleep(0.05)
        osc_msg = liblo.Message('/set_output')
        osc_msg.add(('i', RESET_PIN_INDEX))
        osc_msg.add(('i', 1))      

        liblo.send(self._sensei_address, osc_msg)

        time.sleep(0.5)

