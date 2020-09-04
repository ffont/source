from __future__ import print_function

import argparse

from flask import Flask, render_template, request, redirect, url_for
from oscpy.client import OSCClient

app = Flask(__name__, static_url_path='/static', static_folder='static/')
osc_client = None

sound_parameter_names = ['filterCutoff', 'filterResonance', 'maxPitchRatioMod', 'maxFilterCutoffMod', 'gain']


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
            if (soundIndex == -1):
                soundsToUpdate = list(range(0, 16))
            else:
                soundsToUpdate = [int(soundIndex)]
            for sound_idx in soundsToUpdate:
                for parameter_name in sound_parameter_names:
                    value = float(request.form[parameter_name])
                    osc_client.send_message(b'/set_sound_parameter', [sound_idx, parameter_name, value]) 
                    soundParams[parameter_name] = value
    
    tvars = soundParams.copy()
    tvars.update({
        'query': query,
        'numSounds': numSounds,
        'maxSoundLength': maxSoundLength,
        'midiRootNoteOffset': midiRootNoteOffset,
        'soundIndex': soundIndex
    })
    return render_template("index.html", **tvars)
    

if __name__ == '__main__':
    
    parser = argparse.ArgumentParser()
    parser.add_argument("--ip", default="127.0.0.1", help="The ip to send OSC to")
    parser.add_argument("--osc_port", type=int, default=9000, help="The port to send OSC messages to")
    parser.add_argument("--http_port", type=int, default=8123, help="The port the web server should listen at")
    args = parser.parse_args()

    osc_client = OSCClient(args.ip, args.osc_port, encoding='utf8')
    app.run(port=args.http_port, debug=True)
