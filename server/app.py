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
import urllib.request
from threading import Thread

from flask import Flask, render_template, request, redirect, url_for, Response
from oscpy.client import OSCClient

# TODO: add check to see if we're running in ELK platform, and if it is the case, change template_folder to /home/mind/
template_folder='../SourceSampler/Resources/'
app = Flask(__name__, template_folder=template_folder)  
osc_client = None
plugin_state = "No sate"
time_post_state_triggered = None
time_post_state_received = None


@app.route('/', methods = ['GET'])  # Serve main interface HTML file
def index():        
    tvars = {}
    return render_template("index.html", **tvars)


@app.route('/send_osc', methods=['GET'])  # Forwards the request contents as an OSC message to the plugin
def send_osc():
    address = request.args['address']
    values = [getattr(__builtins__, type_name)(value) for type_name, value in zip(request.args['types'].split(';'), request.args['values'].split(';'))]
    osc_client.send_message(address, values)
    return 'OSC sent'


@app.route('/state_from_plugin', methods = ['POST'])  # Receives a state update from the plugin and saves the contents
def state_from_plugin():
    global plugin_state
    global time_post_state_received
    time_post_state_received = time.time()
    plugin_state = request.data
    return 'State received'


@app.route('/update_state', methods = ['GET'])  # Client requests to get an updated version of the state
def update_state():
    global plugin_state
    global time_post_state_received
    if time_post_state_received is not None and time.time() - time_post_state_received > 5:
        # If older than 5 seconds, plugin is maybe disconnected
        return 'Maybe old'
    else:
        return Response(plugin_state, mimetype='text/xml')


@app.route('/get_system_stats', methods = ['GET'])  # Return system stats (only if running in ELK)
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


class SoundDownloader:
    def __init__(self, url, outfile):
        self.old_percent = 0
        self.url = url
        self.outfile = outfile

    def download_progress_hook(self, count, blockSize, totalSize):
        percent = int(count * blockSize * 100 / totalSize)
        if percent > self.old_percent:
            self.old_percent = percent
            osc_client.send_message('/downloading_sound_progress', [self.outfile, percent])
        if percent >= 100:
            osc_client.send_message('/finished_downloading_sound', [self.outfile])    


def download_sound(url, outfile):
    if not (os.path.exists(outfile) and os.path.getsize(outfile) > 0):
        # If sound does not exist, start downloading
        print('Downloading ' + url)
        progress = SoundDownloader(url, outfile)
        urllib.request.urlretrieve(url, outfile, reporthook=progress.download_progress_hook)
    else:
        # If sound already exists, notify plugin about that
        osc_client.send_message('/finished_downloading_sound', [outfile])    


def download_all_sounds(urls, outfiles):
    for url, outfile in zip(urls, outfiles):
        thread = Thread(target = download_sound, args = (url, outfile, ))
        thread.start()
        #download_sound(url, outfile)


@app.route('/download_sounds', methods = ['GET'])  # Download the sounds requested by the plugin
def download_sounds():
    urls_to_download = []
    outfiles = []
    for url in request.args['urls'].split(','):
        if url:
            sound_id = url.split('/')[-1].split('_')[0]
            outfile = os.path.join(request.args['location'], sound_id + '.ogg')
            urls_to_download.append(url)
            outfiles.append(outfile)
    
    thread = Thread(target = download_all_sounds, args = (urls_to_download, outfiles, ))
    thread.start()

    return 'Downloading async...'


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument("--osc_ip", default="127.0.0.1", help="The IP to send OSC to")
    parser.add_argument("--osc_port", type=int, default=9000, help="The port to send OSC messages to")
    parser.add_argument("--http_port", type=int, default=8123, help="The port the web server should listen at")
    args = parser.parse_args()

    osc_client = OSCClient(args.osc_ip, args.osc_port, encoding='utf8')
    app.run(host='0.0.0.0', port=args.http_port, debug=True)
    print('If running on the Pi, you should be able to access the server at "http://elk-pi.local:{0}"'.format(args.http_port))


# NOTE: this server is only used when running in the ELK platform to download sounds and to server the UI because 
# it seems to be much much faster when doing it outside the plugin. We'll have to investigate if there are ways to 
# do it inside the plugin and getting rid of this.