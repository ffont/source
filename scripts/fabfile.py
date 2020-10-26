import os

from fabric import task, Connection

host = 'mind@elk-pi.local'
remote_dir = '/home/mind/'


@task
def clean(ctx):

    # Remove all intermediate build files (for ELK build)
    os.system("rm -r ../SourceSampler/Builds/ELKAudioOS/build/")

    # Remove all intermediate build files (for macOS build)
    os.system("rm -r ../SourceSampler/Builds/MacOSX/build/")


@task
def send_elk(ctx):

    # Copy compiled file and sushi configuration to board
    print('\nSending compiled SourceSamler and config files to board...')
    print('********************************************************\n')
    with Connection(host=host, connect_kwargs={'password': 'elk'}) as c:
        for local_file, destination_dir in [
            ("start.sh", remote_dir),
            ("source_sushi_config.json", remote_dir),
            ("../server/app.py", remote_dir),
            ("../server/requirements.txt", remote_dir),
            ("glue_app.py", remote_dir),
            ("source_sensei_config.json", remote_dir),
            ("../SourceSampler/Resources/index.html", remote_dir),
            ("../SourceSampler/Builds/ELKAudioOS/build/SourceSampler.so", remote_dir)
        ]:
            print('- Copying {0} to {1}'.format(local_file, destination_dir))
            c.put(local_file, destination_dir)

    print('\nAll done!')
    print('You can now run Source on the ELK board with the command:')
    print('./start.sh')
    print('\n')


@task
def compile_elk(ctx):

    print('Coss-compiling Source for ELK platform...')
    print('*********************************************')

    # Generate binary files
    print('\n* Building BinaryData')
    os.system("../SourceSampler/3rdParty/JUCE/extras/BinaryBuilder/Builds/MacOSX/build/Debug/BinaryBuilder ../SourceSampler/Resources ../SourceSampler/Source/ BinaryData")

    # When compiling in ELK, the JUCE_WEB_BROWSER option of juce_gui_extra needs to be disabled, but we do want it to be
    # enabled when compiling the plugin for other platforms. The solution we adopt is that here we live modify the source files
    # needed to disable the JUCE_WEB_BROWSER option, and then undo the changes

    print('\n* Temporarily modifying makefile')

    def replace_in_file(path, text_to_replace="", replacement=""):
        file_contents = open(path, 'r').read()
        if text_to_replace != "":
            new_file_contents = file_contents.replace(text_to_replace,  replacement)
        else:
            new_file_contents = replacement  # If no text_to_replace is specified, replace the whole file with new contents
        open(path, 'w').write(new_file_contents)
        return file_contents

    old_appconfig_file_contents = replace_in_file("../SourceSampler/JuceLibraryCode/AppConfig.h", 
        text_to_replace="#define   JUCE_WEB_BROWSER 1", 
        replacement="#define   JUCE_WEB_BROWSER 0")

    old_makefile_file_contents = replace_in_file("../SourceSampler/Builds/ELKAudioOS/Makefile", 
        text_to_replace="@pkg-config --print-errors alsa freetype2 webkit2gtk-4.0 gtk+-x11-3.0 libcurl", 
        replacement="@pkg-config --print-errors alsa freetype2 libcurl")

    replace_in_file("../SourceSampler/Builds/ELKAudioOS/Makefile", 
        text_to_replace="JUCE_LDFLAGS += $(TARGET_ARCH) -L$(JUCE_BINDIR) -L$(JUCE_LIBDIR) -L/workdir/sysroots/aarch64-elk-linux/usr/lib/ $(shell pkg-config --libs alsa freetype2 webkit2gtk-4.0 gtk+-x11-3.0 libcurl) -fvisibility=hidden -lrt -ldl -lpthread $(LDFLAGS)", 
        replacement="JUCE_LDFLAGS += $(TARGET_ARCH) -L$(JUCE_BINDIR) -L$(JUCE_LIBDIR) -L/workdir/sysroots/aarch64-elk-linux/usr/lib/ $(shell pkg-config --libs alsa freetype2 libcurl) -fvisibility=hidden -lrt -ldl -lpthread $(LDFLAGS)")
    
    replace_in_file("../SourceSampler/Builds/ELKAudioOS/Makefile", 
        text_to_replace="JUCE_CPPFLAGS := $(DEPFLAGS) -DLINUX=1 -DNDEBUG=1 -DJUCER_LINUX_MAKE_84E12380=1 -DJUCE_APP_VERSION=0.0.1 -DJUCE_APP_VERSION_HEX=0x1 $(shell pkg-config --cflags alsa freetype2 webkit2gtk-4.0 gtk+-x11-3.0 libcurl) -pthread -I$(HOME)/JUCE/modules/juce_audio_processors/format_types/VST3_SDK -I/code/VST2_SDK -I../../JuceLibraryCode -I$(HOME)/JUCE/modules -I/workdir/sysroots/aarch64-elk-linux/usr/include/freetype2/ -I../../3rdParty/cpp-httplib/ -I../../3rdParty/ff_meters/LevelMeter/ $(CPPFLAGS)", 
        replacement="JUCE_CPPFLAGS := $(DEPFLAGS) -DLINUX=1 -DNDEBUG=1 -DJUCER_LINUX_MAKE_84E12380=1 -DJUCE_APP_VERSION=0.0.1 -DJUCE_APP_VERSION_HEX=0x1 $(shell pkg-config --cflags alsa freetype2 libcurl) -pthread -I$(HOME)/JUCE/modules/juce_audio_processors/format_types/VST3_SDK -I/code/VST2_SDK -I../../JuceLibraryCode -I$(HOME)/JUCE/modules -I/workdir/sysroots/aarch64-elk-linux/usr/include/freetype2/ -I../../3rdParty/cpp-httplib/ -I../../3rdParty/ff_meters/LevelMeter/ $(CPPFLAGS)")
    

    # Cross-compile Source
    print('\n* Cross-compiling')
    os.system("find ../SourceSampler/Builds/ELKAudioOS/build/intermediate/Release/ -type f \( \! -name 'include_*' \) -exec rm {} \;")
    os.system('docker run --rm -it -v elkvolume:/workdir -v ${PWD}/../:/code/source -v ${PWD}/../SourceSampler/3rdParty/JUCE_ELK:/home/sdkuser/JUCE -v ${PWD}/../../VST_SDK/VST2_SDK:/code/VST2_SDK -v ${PWD}/custom-esdk-launch.py:/usr/bin/esdk-launch.py crops/extsdk-container')


    # Undo file replacements
    print('\n* Restoring build files')
    replace_in_file("../SourceSampler/JuceLibraryCode/AppConfig.h", replacement=old_appconfig_file_contents)
    replace_in_file("../SourceSampler/Builds/ELKAudioOS/Makefile", replacement=old_makefile_file_contents)
    

    print('\nAll done!')


@task
def compile_macos(ctx):

    # Compile release Source for macOS platform
    print('Compiling Source for macOS platform...')
    print('*********************************************\n')
    os.system("cd ../SourceSampler/Builds/MacOSX/;xcodebuild -configuration Release GCC_PREPROCESSOR_DEFINITIONS='$GCC_PREPROCESSOR_DEFINITIONS JUCER_ENABLE_GPL_MODE=1' LLVM_LTO=NO")    

    print('\nAll done!')


@task
def compile_projucer_macos(ctx):

    # Compile the projucer version compatible with source
    print('Compiling Projucer for macOS platform...')
    print('*********************************************\n')
    os.system("cd ../SourceSampler/3rdParty/JUCE_ELK/extras/Projucer/Builds/MacOSX/;xcodebuild -configuration Release GCC_PREPROCESSOR_DEFINITIONS='$GCC_PREPROCESSOR_DEFINITIONS JUCER_ENABLE_GPL_MODE=1' LLVM_LTO=NO")    

    print('\nAll done!')


@task
def compile(ctx):
    compile_macos(ctx)
    compile_elk(ctx)
    
    
@task
def deploy_elk(ctx):
    compile_elk(ctx)
    send_elk(ctx)
