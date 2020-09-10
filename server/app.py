from __future__ import print_function

import argparse
try:
    import commands
    do_system_stats = True
except:
    # Python 3
    do_system_stats = False
import requests
import os
import time
import sys

from flask import Flask, render_template, request, redirect, url_for, Response
from oscpy.client import OSCClient


app = Flask(__name__, static_url_path='/static', static_folder='static/')
osc_client = None

sound_parameter_names = ['filterCutoff', 'filterResonance', 'maxPitchRatioMod', 'maxFilterCutoffMod', 'gain']
plugin_state = "No sate"
time_post_state_triggered = None
time_post_state_received = None


@app.route('/', methods = ['GET'])
def index():        
    tvars = {}
    return render_template("index.html", **tvars)


@app.route('/download_sounds', methods = ['GET'])
def download_sounds():
    for url in request.args['urls'].split(','):
        if url:
            sound_id = url.split('/')[-1].split('_')[0]
            outfile = '/udata/source/sounds/' + sound_id + '.ogg'
            if not (os.path.exists(outfile) and os.path.getsize(outfile) > 0):
                print('Downloading ' + url)
                r = requests.get(url, allow_redirects=True)
                open(outfile, 'wb').write(r.content)
    return 'All downloaded!'


@app.route('/state_from_plugin', methods = ['POST'])
def state_from_plugin():
    global plugin_state
    global time_post_state_received
    time_post_state_received = time.time()
    plugin_state = request.data
    return 'State received'


@app.route('/update_state', methods = ['GET'])
def update_state():
    global plugin_state
    global time_post_state_received
    if time_post_state_received is not None and time.time() - time_post_state_received > 5:
        # If older than 5 seconds, plugin is maybe disconnected
        return 'Maybe old'
    else:
        return Response(plugin_state, mimetype='text/xml')


@app.route('/trigger_post_state', methods = ['GET'])
def trigger_post_state():
    global time_post_state_triggered
    time_post_state_triggered = time.time()
    osc_client.send_message(b'/post_state', []) 
    return 'Post state triggered'


@app.route('/get_system_stats', methods = ['GET'])
def get_system_stats():
    if do_system_stats and sys.platform != 'darwin':
        # Get system stats (only works when running in ELK as for other host(s) the command would be different)
        temp_out = commands.getstatusoutput("sudo vcgencmd measure_temp")[1]
        cpu_usage = commands.getstatusoutput("grep 'cpu ' /proc/stat | awk '{usage=($2+$4)*100/($2+$4+$5)} END {print usage \"%\"}'")[1]
        mem_usage = commands.getstatusoutput("free | grep Mem | awk '{print $3/$2 * 100.0}'")[1]
        msw = "CPU  PID    MSW        CSW        XSC        PF    STAT       %CPU  NAME\n" + commands.getstatusoutput("more /proc/xenomai/sched/stat | grep sushi_b64")[1]
        return "{0}\ncpu={1}%\nmem={2}%\nMSW:\n{3}".format(temp_out, cpu_usage, mem_usage, msw)
    else:
        return 'No stats'

@app.route('/send_osc', methods=['POST'])
def send_osc():
    address = request.form['address']
    values = [getattr(__builtins__, type_name)(value) for type_name, value in zip(request.form['types'].split(';'), request.form['values'].split(';'))]
    osc_client.send_message(address, values)
    return 'OSC sent'


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument("--ip", default="127.0.0.1", help="The ip to send OSC to")
    parser.add_argument("--osc_port", type=int, default=9000, help="The port to send OSC messages to")
    parser.add_argument("--http_port", type=int, default=8123, help="The port the web server should listen at")
    args = parser.parse_args()

    osc_client = OSCClient(args.ip, args.osc_port, encoding='utf8')
    app.run(host='0.0.0.0', port=args.http_port, debug=True)
    print('If running on the Pi, you should be able to access the server at "http://elk-pi.local:8123"')
