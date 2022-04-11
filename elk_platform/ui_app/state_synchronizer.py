from oscpy.client import OSCClient
from oscpy.server import OSCThreadServer
from helpers import StateNames, configure_recent_queries_sound_usage_log_tmp_base_path_from_source_state
import threading
import asyncio
from bs4 import BeautifulSoup
import time
import sys


sss_instance = None
volatile_state_refresh_fps = 15


def state_update_handler(*values):
    update_type = values[0].decode("utf-8")
    update_id = values[1]
    update_data = values[2:]
    if sss_instance is not None:
        sss_instance.apply_update(update_id, update_type, update_data)
    

def full_state_handler(*values):
    update_id = values[0]
    new_state_raw = values[1]
    if sss_instance is not None:
        sss_instance.set_full_state(update_id, new_state_raw)


def volatile_state_string_handler(*values):
    volatile_state_string = values[0]
    if sss_instance is not None:
        sss_instance.set_volatile_state_from_string(volatile_state_string)


class OSCReceiverThread(threading.Thread):

    def __init__(self, port, source_state_synchronizer, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.port = port
        self.source_state_synchronizer = source_state_synchronizer

    def run(self):
        asyncio.set_event_loop(asyncio.new_event_loop())
        osc = OSCThreadServer()
        osc.listen(address='0.0.0.0', port=self.port, default=True)
        osc.bind(b'/plugin_started', lambda: sss_instance.plugin_has_started())
        osc.bind(b'/plugin_alive', lambda: sss_instance.plugin_is_alive())
        osc.bind(b'/state_update', state_update_handler)
        osc.bind(b'/full_state', full_state_handler)
        osc.bind(b'/volatile_state_string', volatile_state_string_handler)
        print('* Listening OSC messages in port {}'.format(self.port))


class RequestVolatileStateThread(threading.Thread):

    def __init__(self, source_state_synchronizer, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.source_state_synchronizer = source_state_synchronizer

    def run(self):
        asyncio.set_event_loop(asyncio.new_event_loop())
        while True:
            time.sleep(1.0/volatile_state_refresh_fps)
            self.source_state_synchronizer.request_volatile_state()


class SourceStateSynchronizer(object):

    osc_client = None
    last_update_id = -1
    shoud_request_full_state = False
    full_state_requested = False
    state_soup = None
    verbose = False
    last_time_plugin_alive = 0
    volatile_state = {}

    def __init__(self, osc_ip="127.0.0.1", osc_port_send=9001, osc_port_receive=9002, verbose=True):
        global sss_instance
        sss_instance = self
        
        # Start OSC receiver to receive OSC messages from the plugin
        OSCReceiverThread(osc_port_receive, self).start()

        # Start Thread to request volatile state to plugin
        RequestVolatileStateThread(self).start()

        # Start OSC client to send OSC messages to plugin
        self.verbose = verbose
        self.osc_client = OSCClient(osc_ip, osc_port_send, encoding='utf8')
        print('* Sending OSC messages in port {}'.format(osc_port_send))

        time.sleep(0.25)
        self.request_full_state()

    def plugin_has_started(self):
        self.last_update_id = -1
        self.state_soup = None
        self.request_full_state()

    def plugin_is_alive(self):
        self.last_time_plugin_alive = time.time()

    def plugin_may_be_down(self):
        return time.time() - self.last_time_plugin_alive > 5.0  # Consider plugin maybe down if no alive message in 5 seconds

    def request_full_state(self):
        self.osc_client.send_message('/get_state', ["oscFull"])
        self.full_state_requested = True
        self.shoud_request_full_state = False

    def request_volatile_state(self):
        self.osc_client.send_message('/get_state', ["oscVolatileString"])

    def set_full_state(self, update_id, full_state_raw):
        if self.verbose:
            print("Receiving full state with update id {}".format(update_id))
        self.state_soup = BeautifulSoup(full_state_raw, "lxml").findAll("source_state")[0]
        self.full_state_requested = False

        # Configure some stuff that requires the data paths to be known
        configure_recent_queries_sound_usage_log_tmp_base_path_from_source_state(self.state_soup['sourceDataLocation'.lower()], self.state_soup['tmpFilesLocation'.lower()])

    def set_volatile_state_from_string(self, volatile_state_string):
        # Do it from string serialized version of the state
        is_querying, midi_received, last_cc_received, last_note_received, voice_activations, voice_sound_idxs, voice_play_positions, audio_levels = volatile_state_string.decode("utf-8").split(';')
        
        # Is plugin currently querying and downloading?
        self.volatile_state[StateNames.IS_QUERYING] = is_querying != "0"

        # More volatile state stuff
        self.volatile_state[StateNames.VOICE_SOUND_IDXS] = [element for element in voice_sound_idxs.split(',') if element]
        self.volatile_state[StateNames.NUM_ACTIVE_VOICES] = sum([int(element) for element in voice_activations.split(',') if element])
        self.volatile_state[StateNames.MIDI_RECEIVED] = "1" == midi_received
        self.volatile_state[StateNames.LAST_CC_MIDI_RECEIVED] = int(last_cc_received)
        self.volatile_state[StateNames.LAST_NOTE_MIDI_RECEIVED] = int(last_note_received)

        # Audio meters
        audio_levels = audio_levels.split(',') 
        self.volatile_state[StateNames.METER_L] = float(audio_levels[0])
        self.volatile_state[StateNames.METER_R] = float(audio_levels[1])

    def apply_update(self, update_id, update_type, update_data):
        if self.state_soup is not None:
            if self.verbose:
                print("Applying state update {} - {}".format(update_id, update_type))

            if update_type == "propertyChanged":
                tree_uuid = update_data[0].decode("utf-8")
                tree_type = update_data[1].decode("utf-8").lower()
                property_name = update_data[2].decode("utf-8").lower()
                new_value = update_data[3].decode("utf-8")
                results = self.state_soup.findAll(tree_type, {"uuid" : tree_uuid})
                if len(results) == 0:
                    # Found 0 results, initial state is not ready yet so we ignore
                    pass
                elif len(results) == 1:
                    results[0][property_name] = new_value
                else:
                    # Should never return more than one, request a full state as there will be sync issues
                    if self.verbose:
                        print('Unexpected number of results ({})'.format(len(results)))
                    if not self.full_state_requested:
                        self.request_full_state()
            
            elif update_type == "addedChild":
                parent_tree_uuid = update_data[0].decode("utf-8")
                parent_tree_type = update_data[1].decode("utf-8").lower()
                # Only compute child_soup later, if parent is found
                index_in_parent_childs = update_data[3]
                results = self.state_soup.findAll(parent_tree_type, {"uuid" : parent_tree_uuid})
                if len(results) == 0:
                    # Found 0 results, initial state is not ready yet so we ignore
                    pass
                elif len(results) == 1:
                    child_soup =  BeautifulSoup(update_data[2], "lxml")
                    if index_in_parent_childs == -1:
                        results[0].append(child_soup)
                    else:
                        results[0].insert(index_in_parent_childs, child_soup)
                    
                else:
                    # Should never return more than one, request a full state as there will be sync issues
                    if self.verbose:
                        print('Unexpected number of results ({})'.format(len(results)))
                    if not self.full_state_requested:
                        self.request_full_state()
            
            elif update_type == "removedChild":
                child_to_remove_tree_uuid = update_data[0].decode("utf-8")
                child_to_remove_tree_type = update_data[1].decode("utf-8").lower()
                results = self.state_soup.findAll(child_to_remove_tree_type, {"uuid" : child_to_remove_tree_uuid})
                if len(results) == 0:
                    # Found 0 results, initial state is not ready yet so we ignore
                    pass
                elif len(results) == 1:
                    results[0].decompose()
                else:
                    # Should never return more than one, request a full state as there will be sync issues
                    if self.verbose:
                        print('Unexpected number of results ({})'.format(len(results)))
                    if not self.full_state_requested:
                        self.request_full_state()
            

            # Check if update ID is correct and trigger request of full state if there are possible sync errors
            if self.last_update_id != -1 and self.last_update_id + 1 != update_id:
                if not self.full_state_requested:
                    self.shoud_request_full_state = True
                    self.request_full_state()
            self.last_update_id = update_id
