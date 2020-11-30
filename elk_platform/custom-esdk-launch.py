#!/usr/bin/env python

# esdk-launch.py
#
# Copyright (C) 2016 Intel Corporation
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
import argparse
import os
import sys
import glob
import subprocess
import tempfile
import shutil
import stat


class EsdkLaunchError(Exception):
    pass


def download_esdk(url, dest):
    cmd = "curl -# -o {} {}".format(dest, url).split()

    try:
        print "Attempting to download {}".format(url)
        subprocess.check_call(cmd, stdout=sys.stdout, stderr=sys.stderr)
    except subprocess.CalledProcessError:
        errormsg = 'Unable to download "{}".'.format(args.url)
        raise EsdkLaunchError(errormsg)


def setup_esdk(installer, dest):
    cmd = "{} -d {} -y".format(installer, dest).split()
    try:
        subprocess.check_call(cmd, stdout=sys.stdout, stderr=sys.stderr)
    except subprocess.CalledProcessError:
        errormsg = 'Unable to setup sdk.'.format(args.url)
        raise EsdkLaunchError(errormsg)


parser = argparse.ArgumentParser()

parser.add_argument("--url", help="url of the extensible sdk installer")
parser.add_argument("--workdir", default='/workdir',
                    help="Directory containing the prepared extensible sdk. "
                         "Or the location to prepare the sdk if url is "
                         "specfied.")

args = parser.parse_args()

try:
    if not os.path.exists(args.workdir):
        os.mkdir(args.workdir)

    setupscript = glob.glob(os.path.join(args.workdir, "environment-setup-*"))
    esdkfound = setupscript and os.path.exists(os.path.join(args.workdir,
                                                            "sysroots"))

    if esdkfound and args.url:
        errormsg = ('An extensible sdk was found in {} yet "--url" was also '
                    'specified. Cowardly refusing to overwrite existing sdk.')
        errormsg = errormsg.format(args.workdir)
        raise EsdkLaunchError(errormsg)

    elif not esdkfound and not args.url:
        errormsg = ('An extensible sdk was not found in {}. "--url" must be '
                    'specified.')
        errormsg = errormsg.format(args.workdir)
        raise EsdkLaunchError(errormsg)

    if args.url:
        # Add a special mechanism for installing directly from the file
        # rather than trying to download. For example let's say a user had
        # already downloaded a large sdk installer to the workdir. Even
        # copying the file using curl with FILE: might take a while and will
        # also use more space.
        #
        # So if the "url" starts with "/" then treat it as something that
        # should directly be installed.
        if args.url.startswith('/'):
            esdk_installer = args.url
        else:
            tempdir = tempfile.mkdtemp(prefix="esdk-download",
                                       dir=args.workdir)
            esdk_installer = os.path.join(tempdir, "esdk-installer.sh")
            download_esdk(args.url, esdk_installer)

        oldmode = os.stat(esdk_installer).st_mode
        os.chmod(esdk_installer, stat.S_IXUSR | oldmode)

        setup_esdk(esdk_installer, args.workdir)
        setupscript = glob.glob(os.path.join(args.workdir,
                                "environment-setup-*"))

        # Since we don't want the user to have to download a very large image
        # again if the install fails, we don't delete the tmpdir on failure.
        try:
            shutil.rmtree(tempdir, ignore_errors=True)
        except NameError:
            pass

    # Source the environment setup script and run bash
    cmd = 'bash -c'.split()
    args = """
    cd {}; 
    . {};
    cd /code/source/SourceSampler/Builds/ELKAudioOS
    unset LD_LIBRARY_PATH
    source /workdir/environment-setup-aarch64-elk-linux
    export CXXFLAGS="-O3 -pipe -lm -ffast-math -feliminate-unused-debug-types -funroll-loops"
    AR=aarch64-elk-linux-ar make -j2 CONFIG=Release CFLAGS="-DJUCE_HEADLESS_PLUGIN_CLIENT=1 -DELK_BUILD=1 -Wno-psabi" TARGET_ARCH="-mcpu=cortex-a72 -mtune=cortex-a72" -i
    """.format(args.workdir, setupscript[0])
    os.execvp(cmd[0], cmd + [args])

except EsdkLaunchError as e:
    print e
    sys.exit(1)
