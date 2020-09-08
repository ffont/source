from __future__ import print_function

import argparse
import requests
import os
import time

from flask import Flask, render_template, request, redirect, url_for, Response
from oscpy.client import OSCClient

app = Flask(__name__, static_url_path='/static', static_folder='static/')
osc_client = None

sound_parameter_names = ['filterCutoff', 'filterResonance', 'maxPitchRatioMod', 'maxFilterCutoffMod', 'gain']
plugin_state = "No sate"
time_post_state_triggered = None
time_post_state_received = None


@app.route('/', methods = ['GET', 'POST'])
def index():
    query = ''
    numSounds = 16
    maxSoundLength = 0.5
    midiRootNoteOffset = 0
    soundIndex = -1
    soundParams = {
        'filterCutoff': 20000.0,
        'filterResonance': 0.0,
        'maxPitchRatioMod': 0.1,
        'maxFilterCutoffMod': 10.0,
        'gain': 1.0,
    }
    rveerbParamNames = ['roomSize', 'damping', 'wetLevel', 'dryLevel', 'width', 'freezeMode']  # Needed to ensure sorting in dict
    reverbParams = {
        'roomSize': 0.5,
        'damping': 0.5,
        'wetLevel': 0.0,
        'dryLevel': 1.0,
        'width': 1.0,
        'freezeMode': 0.0,
    }
    midiInChannel = 0

    if request.method == 'POST':
        if 'query' in request.form:
            # New query form was submitted
            query = request.form['query']
            numSounds = int(request.form['numSounds'] or '16')
            maxSoundLength = float(request.form['maxSoundLength'] or '0.5')
            osc_client.send_message(b'/new_query', [query, numSounds, maxSoundLength])   
        
        if 'midiRootNoteOffset' in request.form:
            # MIDI offset form was submitted
            midiRootNoteOffset = int(request.form['midiRootNoteOffset']) 
            osc_client.send_message(b'/set_midi_root_offset', [-1 * midiRootNoteOffset])   

        if 'soundIndex' in request.form:
            # Setting sound parameters
            soundIndex = int(request.form['soundIndex'])
            for parameter_name in list(soundParams.keys()):
                value = float(request.form[parameter_name])
                osc_client.send_message(b'/set_sound_parameter', [int(soundIndex), parameter_name, value]) 
                soundParams[parameter_name] = value

        if 'roomSize' in request.form:
            # Setting reverb parameters
            values_list = []
            for parameter_name in rveerbParamNames:
                value = float(request.form[parameter_name])
                reverbParams[parameter_name] = value
                values_list.append(value)
            osc_client.send_message(b'/set_reverb_parameters', values_list) 

        if 'presetName' in request.form:
            # Save current preset
            index = int(request.form['presetNumber'])
            osc_client.send_message(b'/save_current_preset', [request.form['presetName'], index]) 

        if 'loadPresetName' in request.form or 'loadPresetNumber' in request.form:
            # Load preset
            index = int(request.form['loadPresetNumber'])
            osc_client.send_message(b'/load_preset', [request.form['loadPresetName'], index]) 
        
        if 'midiInChannel' in request.form:
            # Set midi in filter
            midiInChannel = int(request.form['midiInChannel']) 
            osc_client.send_message(b'/set_midi_in_channel', [midiInChannel]) 

        if 'hiddenMidiThru' in request.form:
            # Set midi thru
            osc_client.send_message(b'/set_midi_thru', [1 if 'midiThru' in request.form else 0]) 
            
            
    tvars = soundParams.copy()
    tvars.update(reverbParams)
    tvars.update({
        'query': query,
        'numSounds': numSounds,
        'maxSoundLength': maxSoundLength,
        'midiRootNoteOffset': midiRootNoteOffset,
        'soundIndex': soundIndex,
        'midiInChannel': midiInChannel,
    })
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
    print('State from plugin')
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


@app.route('/set_sound_parameter_float', methods = ['GET'])
def set_sound_parameter_float():
    osc_client.send_message(b'/set_sound_parameter', [int(request.args['soundIdx']), request.args['param'], float(request.args['value'])]) 
    return 'Parameter set!'


if __name__ == '__main__':
    
    parser = argparse.ArgumentParser()
    parser.add_argument("--ip", default="127.0.0.1", help="The ip to send OSC to")
    parser.add_argument("--osc_port", type=int, default=9000, help="The port to send OSC messages to")
    parser.add_argument("--http_port", type=int, default=8123, help="The port the web server should listen at")
    args = parser.parse_args()

    osc_client = OSCClient(args.ip, args.osc_port, encoding='utf8')
    app.run(host='0.0.0.0', port=args.http_port, debug=True)
    print('If running on the Pi, you should be able to access the server at "http://elk-pi.local:8123"')
