import os

from fabric import task, Connection

host = 'mind@source.local'
remote_dir = '/udata/source/app/'
remote_vst3_so_dir = '/udata/source/app/SourceSampler.vst3/Contents/aarch64-linux/'

PATH_TO_VST2_SDK_FOR_ELK_CROSS_COMPILATION = '${PWD}/../VST_SDK/VST2_SDK' 


@task
def clean(ctx):

    # Remove all intermediate build files (for ELK build)
    os.system("rm -r SourceSampler/Builds/ELKAudioOS/build/")

    # Remove all intermediate build files (for macOS build)
    os.system("rm -r SourceSampler/Builds/MacOSX/build/")


@task
def send_elk(ctx, include_config_files=False, include_plugin_files=True):

    # Copy compiled file and sushi configuration to board
    # NOTE: note that the architecture folder name of the generated VST3 build (arm64-linux) is in fact wrong,
    # and it should really be "aarch64-linux". The fabric script copies it to the reight directory on the ELK board,
    # but the paths here might need to be updated if the cross compilation toolchain is updated to generate the
    # compiled plugin in the right directory.
    print('\nSending SourceSamler to board...')
    print('********************************\n')
    with Connection(host=host, connect_kwargs={'password': 'elk'}) as c:

        c.run('mkdir -p {}'.format(remote_dir))
        c.run('mkdir -p {}'.format(remote_vst3_so_dir))

        config_files = [
            ("elk_platform/start", remote_dir),
            ("elk_platform/stop", remote_dir),
            ("elk_platform/source_sushi_config.json", remote_dir),
            ("elk_platform/main", remote_dir),
            ("elk_platform/requirements.txt", remote_dir),
            ("elk_platform/LiberationMono-Regular.ttf", remote_dir),
            ("elk_platform/FuturaHeavyfont.ttf", remote_dir),
            ("elk_platform/logo_oled.png", remote_dir),
            ("elk_platform/logo_oled_b.png", remote_dir),
            ("elk_platform/logo_oled_fs.png", remote_dir),
            ("elk_platform/elk_ui_custom.py", remote_dir),
            ("elk_platform/source_states.py", remote_dir),
            ("elk_platform/helpers.py", remote_dir),
            ("elk_platform/freesound_api_key.py", remote_dir),
            ("elk_platform/source_sensei_config.json", remote_dir),
            ("SourceSampler/Resources/index.html", remote_dir),
            ("SourceSampler/Resources/index_minimal.html", remote_dir),
        ]

        plugin_files = [            
            ("SourceSampler/Builds/ELKAudioOS/build/SourceSampler.so", remote_dir + 'SourceSampler.so'),
            #("SourceSampler/Builds/ELKAudioOS/build/SourceSampler.vst3/Contents/arm64-linux/SourceSampler.so", remote_vst3_so_dir)
        ]

        files_to_send = []
        if include_config_files:
            files_to_send += config_files
        if include_plugin_files:
            files_to_send += plugin_files

        for local_file, destination_dir in files_to_send:
            print('- Copying {0} to {1}'.format(local_file, destination_dir))
            c.put(local_file, destination_dir)

        print('Now restarting "sushi" and "source" services in board...')
        c.run('sudo systemctl restart sushi')
        c.run('sudo systemctl restart source')

    print('DONE!')
    print('\n')


@task
def send_elk_all(ctx):
    send_elk(ctx, include_config_files=True)

@task
def send_elk_config(ctx):
    send_elk(ctx, include_config_files=True, include_plugin_files=False)

@task
def send_elk_plugin(ctx):
    send_elk(ctx, include_config_files=False, include_plugin_files=True)


@task
def compile_elk(ctx, configuration='Release'):

    print('Coss-compiling Source for ELK platform...')
    print('*********************************************')

    # Generate binary files
    print('\n* Building BinaryData')
    os.system("SourceSampler/3rdParty/JUCE/extras/BinaryBuilder/Builds/MacOSX/build/Release/BinaryBuilder SourceSampler/Resources SourceSampler/Source/ BinaryData")

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

    old_appconfig_file_contents = replace_in_file("SourceSampler/JuceLibraryCode/AppConfig.h", 
        text_to_replace="#define   JUCE_WEB_BROWSER 1", 
        replacement="#define   JUCE_WEB_BROWSER 0")

    old_makefile_file_contents = replace_in_file("SourceSampler/Builds/ELKAudioOS/Makefile", 
        text_to_replace="libcurl webkit2gtk-4.0 gtk+-x11-3.0) -pthread", 
        replacement="libcurl) -pthread")

    # Cross-compile Source
    # NOTE: for some reason (probably JUCE bug) the copy-step for VST3 in linux can not be disabled and generates permission errors when
    # executed. To fix this issue here we mount a volume where the generated VST3 will be copied. This volume is inside the build folder so
    # it is ignored by git. Hopefully this can be imporved in the future by simply disabling the VST3 copy step
    print('\n* Cross-compiling')
    os.system("find SourceSampler/Builds/ELKAudioOS/build/intermediate/" + configuration + "/ -type f \( \! -name 'include_*' \) -exec rm {} \;")
    os.system('docker run --rm -it -v elkvolume:/workdir -v ${PWD}/:/code/source -v ${PWD}/SourceSampler/Builds/ELKAudioOS/build/copied_vst2:/home/sdkuser/.vst -v ${PWD}/SourceSampler/Builds/ELKAudioOS/build/copied_vst3:/home/sdkuser/.vst3 -v ${PWD}/SourceSampler/3rdParty/JUCE:/home/sdkuser/JUCE -v ' + PATH_TO_VST2_SDK_FOR_ELK_CROSS_COMPILATION + ':/code/VST2_SDK -v ${PWD}/elk_platform/custom-esdk-launch.py:/usr/bin/esdk-launch.py -e CC_CONFIG=' + configuration + ' -e CC_PATH_TO_MAKEFILE=/code/source/SourceSampler/Builds/ELKAudioOS crops/extsdk-container')

    # Undo file replacements
    print('\n* Restoring build files')
    replace_in_file("SourceSampler/JuceLibraryCode/AppConfig.h", replacement=old_appconfig_file_contents)
    replace_in_file("SourceSampler/Builds/ELKAudioOS/Makefile", replacement=old_makefile_file_contents)
    
    print('\nAll done!')


@task
def compile_elk_debug(ctx):
    compile_elk(ctx, configuration='Debug')


@task
def compile_macos(ctx, configuration='Release'):

    # Compile release Source for macOS platform
    print('Compiling Source for macOS platform...')
    print('*********************************************\n')
    os.system("cd SourceSampler/Builds/MacOSX/;xcodebuild -configuration {0}".format(configuration))    

    print('\nAll done!')


@task
def compile_macos_debug(ctx, configuration='Release'):
    compile_macos(ctx, configuration='Debug')


@task
def compile_projucer_macos(ctx):

    # Compile the projucer version compatible with source
    print('Compiling Projucer for macOS platform...')
    print('*********************************************\n')
    os.system("cd SourceSampler/3rdParty/JUCE/extras/Projucer/Builds/MacOSX/;xcodebuild -configuration Release GCC_PREPROCESSOR_DEFINITIONS='$GCC_PREPROCESSOR_DEFINITIONS JUCER_ENABLE_GPL_MODE=1' LLVM_LTO=NO")    

    print('\nAll done!')

@task
def compile_binary_builder_macos(ctx):

    # Compile the BinaryBuilder version compatible with source
    print('Compiling BinaryBuilder for macOS platform...')
    print('*********************************************\n')
    os.system("cd SourceSampler/3rdParty/JUCE/extras/BinaryBuilder/Builds/MacOSX/;xcodebuild -configuration Release GCC_PREPROCESSOR_DEFINITIONS='$GCC_PREPROCESSOR_DEFINITIONS JUCER_ENABLE_GPL_MODE=1' LLVM_LTO=NO")    

    print('\nAll done!')


@task
def compile(ctx):
    compile_macos(ctx)
    compile_elk(ctx)
    
    
@task
def deploy_elk(ctx):
    compile_elk(ctx)
    send_elk(ctx)
