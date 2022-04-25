import os
import time
import urllib

from freesound_interface import get_stored_access_token
from helpers import sizeof_fmt


class SoundDownloaderProgress:
    def __init__(self, url, outfile, sound_uuid, source_plugin_interface):
        self.time_started = time.time()
        self.old_percent = 0
        self.url = url
        self.outfile = outfile
        self.sound_uuid = sound_uuid
        self.source_plugin_interface = source_plugin_interface

    def download_progress_hook(self, count, blockSize, totalSize):
        percent = count * blockSize * 100 / totalSize
        if percent > self.old_percent:
            self.old_percent = percent
            self.source_plugin_interface.send_msg_to_plugin(
                '/downloading_sound_progress', [self.sound_uuid, self.outfile, percent])
        if percent >= 100:
            os.rename(self.outfile + '.tmp', self.outfile)
            self.source_plugin_interface.send_msg_to_plugin(
                '/finished_downloading_sound', [self.sound_uuid, self.outfile, True])
            n_seconds = time.time() - self.time_started
            kbytes_per_second = count * blockSize / n_seconds / 1000
            print('- Finished downloading {} ({} at {:.0f}kbps)'.format(
                self.url, sizeof_fmt(totalSize), kbytes_per_second))


def download_sound(url, outfile, sound_uuid, use_header, source_plugin_interface):
    print('- Downloading ' + url)
    progress = SoundDownloaderProgress(url, outfile, sound_uuid, source_plugin_interface)
    if ':' in use_header:
        opener = urllib.request.build_opener()
        if len(use_header) > len('Authorization: Bearer ' + 10):
            # If the plugin has sent a header long enough so that it contains the actual access token, use it
            opener.addheaders = [(use_header.split(':')[0], use_header.split(':')[1])]
        else:
            # If the plugin did not send an access token, use the one storen in python ui code
            opener.addheaders = [('Authorization', 'Bearer {}'.format(get_stored_access_token()))]
        urllib.request.install_opener(opener)
    try:
        urllib.request.urlretrieve(url, outfile + '.tmp', reporthook=progress.download_progress_hook)
    except urllib.error.ContentTooShortError as e:
        print('ERROR DOWNLOADING AFTER {:.2f} seconds: {}'.format(time.time()-progress.time_started, progress.url))
        print(e)
