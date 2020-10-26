#!/usr/bin/env python3
'''main_app_minimal : minimal glue app example for controlling a synth'''

__copyright__   = 'Copyright (C) 2020 Modern Ancient Instruments Networked AB, dba Elk'
__license__ = "GPL-3.0"

import time

from elkpy import sushicontroller as sc
from elkpy import sushiprocessor as sp

from elk_ui import ElkUIController

FADER_PARAMETERS = [ "Cutoff", "Resonance", "Attack", "Release" ]

class ElkBridge(object):
    """ 
    Bridge / Glue application to connect board UI (pots, buttons, LEDs, etc.)
    to changes in SUSHI's processors.
    """

    def __init__(self):
        self.ui = ElkUIController(self.handle_faders,
                                  self.handle_buttons,
                                  None,
                                  None)

        
        self.sushi = sc.SushiController()
        #self.processor = sp.SushiProcessor("synth", self.sushi)
        
        self._active_param_idx = 0

        self._refresh_display()


    def run(self):
        """ Minimal event loop
        """
        self.ui.run()
                
        while True:
            time.sleep(0.05)
            self.ui.refresh()
            

    def handle_faders(self, idx, val):
        """ Send parameter changes to plugin
            and update OLED display
        """
        self._active_param_idx = idx
        param = FADER_PARAMETERS[idx]
        #self.processor.set_parameter_value(param, val)
        
        self._refresh_display()


    def handle_buttons(self, idx, val):
        """ Send Note ON / OFF messages to SUSHI
            and update LEDs
        """
        # C3 + C scale from C4 to C5
        BUTTON_MIDI_NOTES = [ 48, 60, 62, 64, 65, 67, 69, 71, 72 ]

        self.sushi.send_note_on(0, 0, BUTTON_MIDI_NOTES[idx], val)
        self.ui.set_led(idx, val)


    def _refresh_display(self):
        parameter = FADER_PARAMETERS[self._active_param_idx]
        value = 1.234 #self.processor.get_parameter_value(parameter)
        
        self.ui.set_display_lines(["Param: %s" % parameter, "Value: %.3f" % value])


if __name__ == '__main__':
    bridge = ElkBridge()    
    bridge.run()


