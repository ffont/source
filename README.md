<p align="center">
<img src="docs/source_logo_headline.png" width="500" />
</p>


Table of Contents
=================

   * [Table of Contents](#table-of-contents)
   * [About](#about)
      * [Why making SOURCE?](#why-making-source)
      * [Features](#features)
      * [How it works](#how-it-works)
      * [How can I run SOURCE](#how-can-i-run-source)
         * [Running SOURCE in the ELK hardware stack](#running-source-in-the-elk-hardware-stack)
         * [Running SOURCE as an audio plugin or standalone app in desktop/laptop computers with Linux/macOS](#running-source-as-an-audio-plugin-or-standalone-app-in-desktoplaptop-computers-with-linuxmacos)
   * [Instructions for developers](#instructions-for-developers)
      * [Building SOURCE sampler engine](#building-source-sampler-engine)
         * [Build standalone/plugin for desktop (macOS)](#build-standaloneplugin-for-desktop-macos)
         * [Build plugin for ELK platform](#build-plugin-for-elk-platform)
            * [Prepare ELK development SDK](#prepare-elk-development-sdk)
            * [Prepare VST2 SDK](#prepare-vst2-sdk)
            * [Do the cross-compilation](#do-the-cross-compilation)
            * [Loading the built plugin in the ELK board](#loading-the-built-plugin-in-the-elk-board)
         * [Note about JUCE version used for SOURCE](#note-about-juce-version-used-for-source)
      * [Using the BLACKBOARD simulator in development](#using-the-blackboard-simulator-in-development)
   * [License](#license)



# About

SOURCE is an open-source music sampler powered by [Freesound](https://freesound.org)'s collection of 500k Creative Commons sounds contributed by a community of thousands of people around the world. SOURCE is a sampler that *does not sample*. Instead, it provides different ways to load sounds from Freesound and instantly generate new sound palettes to enrich the creative process and bring an endless SOURCE of inspiration.

SOURCE is designed to run as a stand-alone device on a hardware solution based on a [Raspberry Pi 4](https://www.raspberrypi.org/products/raspberry-pi-4-model-b/), the [Elk Pi](https://elk.audio/extended-dev-kit) hat for the Raspberry Pi (which provides low-latency multi-channel audio I/O), and the [Elk BLACKBOARD](https://elk.audio/blackboard) controller board (which provides the user interface elements including buttons, faders, a display, and the audio I/O connectors). However, the core of SOURCE is implemented as a standard audio plugin using [JUCE](https://juce.com). That allows SOURCE to also be loaded in DAWs that support VST/AU plugins, or even run as a stand-alone and cross-platform application in desktop computers (eventhough with somewhat limited functionality). The picture below shows the looks of SOURCE as deployed with the Elk hardware stack:

<p align="center">
<img src="docs/SOURCE-photo.png" width="500" />
</p>

To see SOURCE in action you can check out this **demo video**: https://youtu.be/7EXMY0AvBxo
Also, I published a paper about source in the **2nd International Workshop on the Internet of Sounds**, part of the **Audio Mostly 2021** conference. I'm happy to say that SOURCE got the the **Best Demo Award** and the **Industry Award** (awared by ELK) in Audio Mostly 2021. Here is the citation information for the paper (and the [PDF](https://repositori.upf.edu/bitstream/handle/10230/48719/font_am21_source.pdf?sequence=1&isAllowed=y)):

```
Font F. "SOURCE: a Freesound community music sampler." In: Audio mostly, A conference on interaction with sound. Proceedings of the 16th International Audio Mostly Conference AM’ 21; 2021 Sep 1-3; Trento, Italy. New York: Association for Computing Machinery; 2021. p. 182-7. DOI: 10.1145/3478384.3478388 
```


## Why making SOURCE? 

As a researcher at the [Music Technology Group](https://www.upf.edu/web/mtg/) of [Universitat Pompeu Fabra](https://www.upf.edu), I have been leading the development of the Freesound website and coordinating research projects around it for a number of years. I've been always interested in how to take advantage of the creative potential of Freesound's huge sound collection, and in ways to better integrate Freesound in the creative process. Even though 
SOURCE started as a personal side project (that's why the [Rita & Aurora](https://ritaandaurora.github.io) logo is shown below, a fancy name I sometimes use for audio dev side-projects), I soon realized about the potential of the prototype and saw that SOURCE can bring together many of the research ideas that we have been experimenting with at the MTG in the last years. I believe that SOURCE can be a great music-making tool for creators, but I also think it can be a great playground for experimentation and research about the interaction between hardware devices and huge sound collections like Freesound.


## Features

SOURCE implements audio playback functionality which is common in many existing music samplers. Perhaps more interesting and unique are the capabilities of SOURCE for interacting with Freesound, searching and retrieving sounds. The SOURCE demonstration video linked above showcases some of these possibilities. Here is a (potentially incomplete) list of features:

* Search sounds in Freesound in real-time and download them to the sampler
* Filter sounds by:
 * Textual query terms
 * Duration
 * Creative Commons license
 * Perceptual qualities like: *brightness*, *hardness* and *depth*
* Replace loaded sounds by other sounds that are acoustically similar (using Freesound's similarity search feature)
* Random search mode that will retrieve unexpected sounds from Freesound
* Load any number of sounds (only limited by RAM memory)
* Map loaded sounds to MIDI notes automatically using *contiguous* or *interleaved* modes, or map them arbitrarily using a mapping editor
* For each sound loaded, configure sound paramters such as:
 * Start and end position
 * Play modes including looping and slicing
 * Loop start and end positions
 * ADSR amplitude envelope
 * Low-pass filter with ADSR envelope
 * MIDI root note and global pitch shift (based on playback speed)
 * Freeze mode in which the playhead position can be controlled as a sound parameter
 * Modulation of some of the above parameters with velocity and aftertouch (including support for polyhonic aftertouch)
 * Control some of the above parameters with MIDI Control Change
* Get a *sound usage log* which lists the historic of sounds that have been used and can help in the Creative Commons attribution process

Note that the most interesting bit of SOURCE is it's methods for interacting with Freesound. The audio engine itself is rather basic and, well, those experience music sampler developers might find it naive. I'm open to contributions if anyone with more experience implementing music samplers wants to help :)

## How it works

SOURCE is composed of a number of software processes that run on a hardware solution based on a [Raspberry Pi 4](https://www.raspberrypi.org/products/raspberry-pi-4-model-b/), the [Elk Pi](https://elk.audio/extended-dev-kit) hat for the Raspberry Pi (which provides low-latency multi-channel audio I/O), and the [Elk BLACKBOARD](https://elk.audio/blackboard) controller board (which provides the user interface elements including buttons, faders, a display, and the audio I/O connectors). All software processes run under [Elk Audio OS](https://elk.audio/audio-os), an operative system optimized for low-latency and real-time audio systems. The core of SOURCE is the *sampler engine* which is implemented as a VST plugin and is run by the *sushi* process (a plugin host bundled with Elk Audio OS). The communication with the sensors of the controller board is carried out by the *sensei* process, which is also part of the Elk Audio OS distribution. Finally, a *glue app* is responsible for connecting all the sub-systems together (mostly via Open Sound Control), controlling the state of the user interface, exposing an HTTP endopoint that offers a complementary user interface, and, most importantly, communicating with Freesound to search and download sounds. Below there is a block diagram including all the aforementioned software processes and hardware elements. 

<p align="center">
<img src="docs/SOURCE-main-diagram.png" width="400" />
</p>


## How can I run SOURCE

### Running SOURCE in the ELK hardware stack

The run SOURCE in the ELK platform you'll need to configure services to run the `sushi` process with the built sampler engine, the `sensei` process with the BLACKBOARD sensors configuration settings, and finally the `source` process which runs the glue app which connects all systems together, manages the user interface and connects to Freesound (indluding downloading sounds). To do that follow the instructions below. Note that this assumes `ssh` connection to the RPi as described in Elk documentation, and Elk Audio OS 0.7.2 installed. Note that most of the steps are to be run from your local/devleopment computer. This whole process could be simplified with more work on the *deploy scripts*.


 1. Clone the source code repository in your local computer

```
git clone https://github.com/ffont/source.git && cd source && git submodule update --init
```

 2. Get a Freesound API key (https://freesound.org/apiv2/apply) and create a file named `freesound_api_key.py` inside the `elk_platform` folder of the cloned repository with the contents:

```
FREESOUND_API_KEY = "YOUR FREESOUND API KEY"
FREESOUND_CLIENT_ID = "YOUR FREESOUND CLIENT ID"
```

 3. Download the *pre-compiled binaries* of the latest release of SOURCE sampler engine for the ELK platform (or [build them locally following the instructions in sections below](#build-plugin-for-elk-platform)). Copy `SourceSampler.so` to `SourceSampler/Builds/ELKAudioOS/build/SourceSampler.so` (you might need to create intermediate folders). Note that this path is relative to the root of the cloned repository. 
 4. In your local machine, install `fabric` Python dependency with `pip install fabric>=2.6.0`. This is needed in the following steps to run the scripts that partially automate the deployment of SOURCE in the RPi board.
 5. In the RPi, create the folder `/udata/source/app/` (and intermediate folders if needed).
 6. Modify the `host` variable in `fabfile.py` to match the host name of your RPi board. If you did not modify the host name of the RPi, you should use `mind@elk-pi.local`, but note that `fabfile.py` has a customized host name.
 7. From your local machine and located in the root of the source code repository, run `fab send-elk-all` to copy necessary files to the RPi board for SOURCE to run. This will also try to run some things and you might see some errors on screen, but it is fine at this point.
 8. From the RPi board, install Python dependencies with `cd /udata/source/app/; pip3 install -r requirements.txt` (need to use the Python 3 installation present in Elk Audio OS).
 9. From the RPi board, give more permissons to `mind` user as [described below](#give-more-sudo-permissions-to-mind-user).
 10. From the RPi board, enable the installed `systemd` services for SOURCE:

```
sudo systemctl enable sushi
sudo systemctl enable sensei
sudo systemctl enable source
```

 11. From the local computer, run again `fab send-elk-all`. This should restart all the SOURCE related services and you should get SOURCE up and running after that.

Once these steps are finished, SOURCE should automatically be run at startup. You can check the logs of the individual processes like you'd do with other processes managed by `systemd`:

```
sudo journalctl -fu sushi
sudo journalctl -fu sensei
sudo journalctl -fu source
```

Note that to get **MIDI input** you'll need to use a USB MIDI controller or interface, and might need to [edit this line of code](https://github.com/ffont/source/blob/7fd268d3dfa7daf2e65182c67faadf598d4d196f/elk_platform/main#L144) and set the id of the USB MIDI controller/interface.

#### Give more sudo permissions to "mind" user

To run SOURCE, user `mind` needs to have `sudo` control without entering password. This is mainly because of the `collect_system_stats` routine called in the `main` glue app. However, it looks like wihtout having sudo capabilities,
running `sensei` might also fail. To add sudo capabilities:

 * switch to user `root` (`sudo su`)
 * edit `/etc/sudoers` to add the line `mind ALL=(ALL) NOPASSWD: ALL` in the *User privilege specification* AND comment the line `# %sudo ALL=(ALL) ALL`. **NOTE**: this gives too many permissions to `mind` user and should be fixed to give only what is needed.


### Running SOURCE as an audio plugin or standalone app in desktop/laptop computers with Linux/macOS

In order to run SOURCE as an audio plugin in a desktop or laptop computer with Linux or macOS, you can simply download the binary files from the [releases](https://github.com/ffont/source/releases) section and copy them to the appropriate audio plugin system locations (that is to copy `SourceSampler.component` and/or `SourceSampler.vst3` files for macOS,  and `SourceSampler.so` and/or `SourceSampler.vst3` for Linux). On macOS, SOURCE can also be run as a standalone application by opening the `SourceSampler.app` bundle. Note that **the audio plugin version of SOURCE has limited functionality**, as the *glue app* is not running and only very basic interaction with Freesound and control of the sampler engine is provided. Below is a screenshot of the SOURCE user interface when running as an audio plugin.

<p align="center">
<img src="docs/SOURCE-complementary-UI.png" width="500" />
</p>


# Instructions for developers


## Building SOURCE sampler engine

The sampler engine of source SOURCE is implemented as a JUCE audio plug-in and can be edited and built using standard JUCE workflows. The first step is to clone this repository and init the submodules. Then, different steps apply to build SOURCE for desktop computers or for the ELK platform (see below).

```
git clone https://github.com/ffont/source.git && cd source && git submodule update --init
```


### Build standalone/plugin for desktop (macOS)

For development purposes (or to run SOURCE in a desktop computer insead of the ELK platform), you can use the XCode project files generated by Projucer. You can also generate exporters for other platforms using Projucer, but I only tested macOS.

Alternatively, you can also use the Python *Fabric 2* script (see below) provided im the `scripts` folder. Do it like that:

```
fab compile-macos
```

This will create *Release* versions of SOURCE (VST3, VST2, AU and Standalone) ready to work on the mac. If you need *Debug* build, you can run `fab compile-macos-debug`.

**NOTE**: macOS build targets include a *pre-build shell script* phase which generates the `BinaryData.h/.cpp` files needed for the plugin to include up-to-date resources (mainly `index.html`). These files are generated with the `BinaryBuilder` util provided in the JUCE codebase. `BinaryBuilder` is compiled as part of the build process so you should encounter no issues with that.

**NOTE 2**: macOS build targets require `openssl` to implement the HTTPS sevrer that hosts the plugin UI. Install by using `brew install openssl`.

**NOTE 3**: SOURCE requires a Freesound API key to connect to Freesound. You should make an account in Freesound (if you don't have one) and go to [this URL to generate an APi key](https://freesound.org/apiv2/apply). Then you should edit the file `/source/SourceSampler/Source/api_key.example.h`, add your key, and then then save and rename the file to `/source/SourceSampler/Source/api_key.h`.

**NOTE 4**: For development you might need to edit the `SourceSampler.jucer` Projucer file. To do that, you need a compatible version of Projucer installed. You can compile it (for macOS) from JUCE source files using a the *Fabric 2* Python script running: `fab compile-projucer-macos`. The generated executable will be in `/source/SourceSampler/3rdParty/JUCE/extras/Projucer/Builds/MacOSX/build/Release/Projucer.app`.

**NOTE 5**: It should be very easy to adapt these instructions for Windows or Linux. You'll basically need to create an exporter for these other platforms an make small changes to compile `BinaryBuilder`, `Projucer` and install/link the dependencies.

**NOTE 6**: SOURCE is configured to build a VST2 version of the plugin (together with VST3, AudioUnit and StandAlone). VST2 is currently only needed for the ELK build as there still seem to be some issues with JUCE6 + VST3 in linux. However, VST is not really needed for the macOS compilation. If you don't have the VST2 SDK, just open `SourceSampler.jucer` and untick `VST Legacy` option.


### Build plugin for ELK platform

To build SOURCE for ELK Audio OS you need to cross-compile it from your development computer. To do that, I use a Docker-based solution on macOS. The instructions here are therefore for cross-compiling from macOS and using Docker. For cross-compilation from Linux it should be simpler and you should refer to the ELK docs.

To do the cross compilation (and also deployment to the board) I prepared a Python script using [Fabric 2](http://www.fabfile.org) so I assume you have a Python 3 interpreter installed in your system with the Fabric 2 package installed (`pip install fabric` should do it).


#### Prepare ELK development SDK

The first thing to do is to prepare the ELK development SDK Docker image following the [instrucitons here](https://github.com/elk-audio/elkpi-sdk/blob/master/running_docker_container_on_macos.md). You need to run steps 1 to 3, no need to run the toolchain when everything installed.

#### Prepare VST2 SDK

Even though JUCE 6 has support for VST3 plugins in Linux, I've had some issues with VST3 versions of plugins in Linux and therefore SOURCE is still being built as VST2. This means that you need the VST2 SDK installed in your computer to compile SOURCE. Make sure that the `PATH_TO_VST2_SDK` variable in `fabfile.py` points to a valid distribution of the VST2 SDK. This will be mounted in the Docker container that does the cross-compilation.

#### Do the cross-compilation

With all this in place, you should be able to cross-compile SOURCE for ELK in a very similar way as you would do it for the desktop, using the Fabric script:

```
fab compile-elk
```

(if you need a *Debug* build, you can use `fab compile-elk-debug`)

This will take a while, specially the first time it runs. When it finishes, it should have generated a `SourceSampler.so` file in `source/Builds/ELKAudioOS/build/` which is the VST2 plugin that you can run in the ELK platform. It also generates a VST3 in `source/Builds/ELKAudioOS/build/SourceSampler.vst3` which should also compatible with the ELK platform but as I mentioend before it does not seem to work properly.

**NOTE**: The build script for the cross compilation includes a step which generates the `BinaryData.h/.cpp` files needed for the plugin to include up-to-date resources (mainly `index.html`). This is run in the host machine and not in the Docker container. For this step to succeed, you need to compile the  `BinaryBuilder` util provided by JUCE. You can compile that using the project files you'll find in `/source/SourceSampler/3rdParty/JUCE/extras/BinaryBuilder/Builds/` or by running `fab compile-binary-builder-macos`.


#### Loading the built plugin in the ELK board

You can send the plugin to the board using the command:

```
fab send-elk
```

This will send both the generated plugin files and other SOURCE configuration files.


### Note about JUCE version used for SOURCE

The current version of SOURCE uses JUCE 6 which has native support for VST3 plugins in Linux and for headless plugins. Therefore, unlike previous version of SOURCE, we don't need any patched version of JUCE and we can simply use the official release :) However, there still seem to be problems with VST3 and Linux, so we use VST2 
builds.

 
## Using the BLACKBOARD simulator in development

TODO...
    
    
# License

SOURCE is released under the **GPLv3** open source software license (see [LICENSE](https://github.com/ffont/source/blob/master/LICENSE) file) with the code being available at  [https://github.com/ffont/source](https://github.com/ffont/source). Source uses the following open source software libraries: 

* [juce](https://juce.com), available under GPLv3 license ([@7c797c8](https://github.com/juce-framework/JUCE/commit/2f980209cc4091a4490bb1bafc5d530f16834e58), v6.1.5)
* [cpp-httplib](https://github.com/yhirose/cpp-httplib), available under MIT license ([@3da4a0a](https://github.com/yhirose/cpp-httplib/tree/3da4a0a))
* [ff_meters](https://github.com/ffAudio/ff_meters), available under BSD 3 clause license ([@711ee87](https://github.com/ffont/ff_meters/tree/711ee87862e1c2485536e977ab57b1f78b84667f), I use a fork I made with a small patch for compatibility with ELK)
* [twine](https://github.com/elk-audio/twine), available under GPLv3 license ([@1257d93](https://github.com/elk-audio/twine/tree/1257d93882cf9fd120539a2ce5497fcbef22af82))
* [asio](https://github.com/chriskohlhoff/asio), available under Boost Sofrware License] ([@f0a1e1c](https://github.com/chriskohlhoff/asio/tree/f0a1e1c7c0387ad16358c81eb52528f190df625c))
* [Simple-WebSocket-Server](https://gitlab.com/eidheim/Simple-WebSocket-Server), available under MIT license ([@a091e7c](https://gitlab.com/eidheim/Simple-WebSocket-Server/-/tree/a091e7cfb1587e3c0340bc7d2d850a4e44c03e11))

<br><br>
<p align="center">
<img src="docs/upf_logo_web.png" width="250" />
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
<img src="docs/ra_logo_web.png" width="250" />
</p>
