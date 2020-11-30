# Source, a hardware Freesound-powered sampler


## Building Source

Source is implemented as a JUCE audio plug-in and can be edited and built using standard JUCE workflows. The first step is to clone this repository and init the submodules. Then, different steps apply to build Source for desktop computers or for the ELK platform (see below).

```
git clone https://github.com/ffont/source.git && cd source && git submodule update --init
```


### Build standalone/plugin for desktop (macOS)

For development purposes (or to run Source in a desktop computer insead of the ELK platform), you can use the XCode project files generated by Projucer. You can also generate exporters for other platforms using Projucer, but I only tested macOS.

Alternatively, you can also use the Python *Fabric 2* script (see below) provided im the `scripts` folder. Do it like that:

```
cd scripts
fab compile-macos
```

This will create *Release* versions of Source (VST3, VST2, AU and Standalone) ready to work on the mac.

**NOTE**: macOS build targets include a *pre-build shell script* phase which generates the `BinaryData.h/.cpp` files needed for the plugin to include up-to-date resources (mainly `index.html`). These files are generated with the `BinaryBuilder` util provided in the JUCE codebase. `BinaryBuilder` is compiled as part of the build process so you should encounter no issues with that.

**NOTE 2**: macOS build targets require `openssl` to implement the HTTPS sevrer that hosts the plugin UI. Install by using `brew install openssl`.

**NOTE 3**: Source requires a Freesound API key to connect to Freesound. You should make an account in Freesound (if you don't have one) and go to [this URL to generate an APi key](https://freesound.org/apiv2/apply). Then you should edit the file `/source/SourceSampler/Source/api_key.example.h`, add your key, and then then save and rename the file to `/source/SourceSampler/Source/api_key.h`.

**NOTE 4**: For development you might need to edit the `SourceSampler.jucer` Projucer file. To do that, you need a compatible version of Projucer installed. You can compile it (for macOS) from JUCE source files using a the *Fabric 2* Python script running: `fab compile-projucer-macos`. The generated executable will be in `/source/SourceSampler/3rdParty/JUCE/extras/Projucer/Builds/MacOSX/build/Release/Projucer.app`.

**NOTE 5**: it should be very easy to adapt these instructions for windows or linux. You'll basically need to create an exporter for these other platforms an make small changes to compile `BinaryBuilder`, `Projucer` and install/link the dependencies.


### Build plugin for ELK platform

To build Source for ELK Audio OS you need to cross-compile it from your development computer. To do that, I use a Docker-based solution on macOS. The instructions here are therefore for cross-compiling from macOS and using Docker. For cross-compilation from Linux it should be simpler and you should refer to the ELK docs.

To do the cross compilation (and also deployment to the board) I prepared a Python script using [Fabric 2](http://www.fabfile.org) so I assume you have a Python 3 interpreter installed in your system with the Fabric 2 package installed (`pip install fabric` should do it).


#### Prepare ELK development SDK

The first thing to do is to prepare the ELK development SDK Docker image following the [instrucitons here](https://github.com/elk-audio/elkpi-sdk/blob/master/running_docker_container_on_macos.md). You need to run steps 1 to 3, no need to run the toolchain when everything installed.


#### Preparing some code dependencies

Source needs to be built as a VST2 plugin, meaning that you need the VST2 SDK installed in your computer. This is because the JUCE version used (5.4.7), does not support building VST3 plugins for Linux. The Python script used to build Source will try to mount the VST2 SDK in the Docker container so it can be used for compilation. The script will search for a folder named `VST_SDK` (the SDK) at the same directory level where the `source` folder is (if needed, you can edit this in `fabfile.py`). You should find a copy of the VST2 SDK and place it in the corresponding directory.


#### Do the cross-compilation

With all this in place, you should be able to cross-compile Source for ELK in a very similar way as you would do it for the desktop, using the Fabric script:

```
cd scripts
fab compile-elk
```

This will take a while, specially the first time it runs. When it finished, it should have generated a `SourceSampler.so` file in `source/Builds/ELKAudioOS/build/` which is the VST2 plugin that you can run in ELK platform.

**NOTE**: the build script for the cross compilation includes a step which generates the `BinaryData.h/.cpp` files needed for the plugin to include up-to-date resources (mainly `index.html`). This is run in the host machine and not in the Docker container. For this step to succeed, you need to compile the  `BinaryBuilder` util provided by JUCE. You can compile that using the project files you'll find in `/source/SourceSampler/3rdParty/JUCE/extras/BinaryBuilder/Builds/`.


#### Loading the built plugin in the ELK board

Instructions for this section still need to be added.


### Note about JUCE version used for Source

To build Source for the ELK platform Source it needs a custom version of JUCE which is patched to remove some graphical dependencies. In particular, we use a patched version of JUCE 5.4.7 which can be found in my [own fork of JUCE](https://github.com/ffont/JUCE_ELK) (a branch named [`juce_547_76910b0eb_ELK`](https://github.com/ffont/JUCE_ELK/tree/juce_547_76910b0eb_ELK)). The patch basically cosists in adding a number of `#if/#endif` to avoid using graphical libraries not present in ELK cross-compilation toolchain. I basically ported the [official patch proposed by ELK for JUCE version 5.4.5](https://github.com/juce-framework/JUCE/compare/master...elk-audio:mind/headless_plugin_client_next) to JUCE 5.4.7. The patched JUCE version can be used to build macOS version normally.

The custom JUCE patch is included as a submodule of this repository (`source/SourceSampler/3rdParty/JUCE_ELK`) and is already configured in the Projucer file. For conveniency, the standard JUCE version (at the same commit as the patched one) is also included as a submodule (`source/SourceSampler/3rdParty/JUCE`), but it is only used by the build scritps to build `BinaryBuilder` and `Projcuer`.

As far as I know, JUCE 6 is not yet supported by ELK, so we're using 5.4.7 here. Once JUCE 6 becomes supported, we won't probably need the cusotm patch (because JUCE 6 supports headless plugins) and also we won't need the VST2 SDK anymore because JUCE 6 can build VST3 plugins on Linux platforms.

### Auto-start notes for ELK platform

Follow official ELK instructions here: https://elk-audio.github.io/elk-docs/html/documents/working_with_elk_board.html#configuring-automatic-startup

1) Modify `sushi` service (`/lib/systemd/system/sushi.service`) to point to `/home/mind/source_sushi_config.json`. Change line:

```
Environment=LD_LIBRARY_PATH=/usr/xenomai/lib
ExecStart=/home/mind/sushi -r --multicore-processing=2 -c /home/mind/source_sushi_config.json
```

2) Modify `sensei` service (`/lib/systemd/system/sensei.service`) to point to `/home/mind/source_sushi_config.json`. Change line:

```
Environment=LD_LIBRARY_PATH=/usr/xenomai/lib
ExecStart=/usr/bin/sensei -f /home/mind/source_sensei_config.json
```

3) Create new service for python server at `/lib/systemd/system/source.service` (use `sudo`). This server is used as the "glue" app which will run the HTTP server as well as control hardware stuff (display, LEDs, etc) and communicate with the plugin running in sushi using OSC messages. The server 
also takes care of downloading sounds as it seems to be much faster to do it from outside sushi.

```
[Unit]
Description=source web server
After=load-drivers.service

[Service]
Type=simple
RemainAfterExit=yes
WorkingDirectory=/home/mind/
ExecStart=main
User=mind

[Install]
WantedBy=multi-user.target
```

4) Enable `sushi`, `sensei`, and `source` services

```
sudo systemctl enable sushi
sudo systemctl enable sensei
sudo systemctl enable source
```

Now both services will start automatically on start up. You can still use `./start` to test new versions because the previous processes will be killed.

When running from startup, you can check std out logs with:

```
sudo journalctl -fu sushi
sudo journalctl -fu sensei
sudo journalctl -fu source
```
    
    
## Licenses

Source is released under the **GPLv3** open source software license (see [LICENSE](https://github.com/ffont/source/blob/master/LICENSE) file) with the code being available at  [https://github.com/ffont/source](https://github.com/ffont/source). Source uses the following open source software libraries: 

* [juce](https://juce.com), available under GPLv3 license ([@76910b0](https://github.com/juce-framework/JUCE/tree/76910b0ebd7fd00d7d03b64beb6f5c96746cf8ce), v5.4.7)
* [cpp-httplib](https://github.com/yhirose/cpp-httplib), available under MIT license ([@3da4a0a](https://github.com/yhirose/cpp-httplib/tree/3da4a0a))
* [ff_meters](https://github.com/ffAudio/ff_meters), available under BSD 3 clause license ([@71e05b8](https://github.com/ffAudio/ff_meters/commit/71e05b8f93ec3643fc7268b8da65d7b98bdefdf8))

