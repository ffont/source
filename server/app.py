from __future__ import print_function

import argparse
import requests
import os
import time
import sys

from flask import Flask, request

app = Flask(__name__)


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


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument("--http_port", type=int, default=8123, help="The port the web server should listen at")
    args = parser.parse_args()
    app.run(host='0.0.0.0', port=args.http_port, debug=True)


# NOTE: this server is only used when running in the ELK platform to download sounds because it seems to be much much
# faster when doing it outside the plugin. We'll have to investigate if there are ways to do it inside the plugin
# and getting rid of this.