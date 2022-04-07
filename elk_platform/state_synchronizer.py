from oscpy.client import OSCClient
from oscpy.server import OSCThreadServer
import threading
import asyncio
import argparse
from bs4 import BeautifulSoup
import sys
import time

sm = None


class StateManager(object):

    osc_client = None
    last_update_id = -1
    shoud_request_full_state = False
    full_state_requested = False
    state_soup = None

    def __init__(self, osc_ip, osc_port_send):
        # Start OSC client to send OSC messages to plugin
        self.osc_client = OSCClient(osc_ip, osc_port_send, encoding='utf8')
        print('* Sending OSC messages in port {}'.format(osc_port_send))

    def plugin_has_started(self):
        self.last_update_id = -1
        self.state_soup = None
        self.request_full_state()

    def request_full_state(self):
        self.osc_client.send_message('/get_state', ["oscFull"])
        self.full_state_requested = True
        self.shoud_request_full_state = False

    def set_full_state(self, update_id, full_state_raw):
        print("Receiving full state with update id {}".format(update_id))
        self.state_soup = BeautifulSoup(full_state_raw, "lxml").findAll("source_state")[0]
        self.full_state_requested = False

    def apply_update(self, update_id, update_type, update_data):
        if sm.state_soup is not None:
            print("Applying state update {} - {}".format(update_id, update_type))

            # Apply change
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
                    print('Unexpected number of results ({})'.format(len(results)))
                    if not self.full_state_requested:
                        self.request_full_state()
            

            # Check if update ID is correct and trigger request of full state if there are possible sync errors
            if self.last_update_id != -1 and self.last_update_id + 1 != update_id:
                if not self.full_state_requested:
                    self.shoud_request_full_state = True
                    self.request_full_state()
            self.last_update_id = update_id
        
        
def state_update_handler(*values):
    update_type = values[0].decode("utf-8")
    update_id = values[1]
    update_data = values[2:]
    sm.apply_update(update_id, update_type, update_data)
    

def full_state_handler(*values):
    update_id = values[0]
    new_state_raw = values[1]
    sm.set_full_state(update_id, new_state_raw)


class OSCReceiverThread(threading.Thread):

    def __init__(self, port, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.port = port

    def run(self):
        asyncio.set_event_loop(asyncio.new_event_loop())
        osc = OSCThreadServer()
        sock = osc.listen(address='0.0.0.0', port=self.port, default=True)
        osc.bind(b'/plugin_started', lambda: sm.plugin_has_started())
        osc.bind(b'/state_update', state_update_handler)
        osc.bind(b'/full_state', full_state_handler)
        print('* Listening OSC messages in port {}'.format(self.port))


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--osc_ip", default="127.0.0.1", help="The IP to send OSC to")
    parser.add_argument("--osc_port_send", type=int, default=9001, help="The port to send OSC messages to")
    parser.add_argument("--osc_port_receive", type=int, default=9002, help="The port to send OSC messages to")
    args = parser.parse_args()

    # Start OSC receiver to receive OSC messages from the plugin
    OSCReceiverThread(args.osc_port_receive).start()

     # Initialize state manager
    sm = StateManager(args.osc_ip, args.osc_port_send)

    time.sleep(0.25)
    sm.request_full_state()

    while True:
        time.sleep(5)
