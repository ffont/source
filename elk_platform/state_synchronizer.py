from oscpy.client import OSCClient
from oscpy.server import OSCThreadServer
import threading
import asyncio
import argparse
from bs4 import BeautifulSoup
import sys
import time

sm = None
osc_client = None


class StateManager(object):

    def __init__(self, *args, **kwargs):
        self.state_xml = BeautifulSoup()

    def set_full_state(self, full_state_xml):
        self.state_xml = full_state_xml



def state_update_handler(values):
    print("Receiving state update...")
    print(values)
    


def full_state_handler(values):
    print("Receiving full state...")
    state = BeautifulSoup(values, "lxml")
    sm.set_full_state(state.findAll("source_state")[0])
    print(sm.state_xml.prettify())
    


class OSCReceiverThread(threading.Thread):

    def __init__(self, port, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.port = port

    def run(self):
        asyncio.set_event_loop(asyncio.new_event_loop())
        osc = OSCThreadServer()
        sock = osc.listen(address='0.0.0.0', port=self.port, default=True)
        osc.bind(b'/state_update', state_update_handler)
        osc.bind(b'/full_state', full_state_handler)
        print('* Listening OSC messages in port {}'.format(self.port))


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--osc_ip", default="127.0.0.1", help="The IP to send OSC to")
    parser.add_argument("--osc_port_send", type=int, default=9001, help="The port to send OSC messages to")
    parser.add_argument("--osc_port_receive", type=int, default=9002, help="The port to send OSC messages to")
    args = parser.parse_args()

    # Start OSC client to send OSC messages to plugin
    print('* Sending OSC messages in port {}'.format(args.osc_port_send))
    osc_client = OSCClient(args.osc_ip, args.osc_port_send, encoding='utf8')

    # Start OSC receiver to receive OSC messages from the plugin
    OSCReceiverThread(args.osc_port_receive).start()

     # Initialize state manager
    sm = StateManager()

    time.sleep(0.25)
    osc_client.send_message('/get_state', ["oscFull"])

    while True:
        pass