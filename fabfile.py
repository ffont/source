import os
import platform
import time

from bs4 import BeautifulSoup as bs

from fabric import task, Connection


host = 'mind@source.local'
remote_base_dir = '/udata/source/'
ui_app_requirements_remote_path = os.path.join(remote_base_dir, 'ui_app', 'requirements.txt')
local_ui_app_path = 'elk_platform/ui_app'
local_config_files_path = 'elk_platform/config'
local_vst2_elk_binary_path = 'SourceSampler/Builds/ELKAudioOS/build/SourceSampler.so'


PATH_TO_VST2_SDK_FOR_ELK_CROSS_COMPILATION = '${PWD}/../VST_SDK/VST2_SDK' 


def sudo_install(connection, source, dest, *, owner='root', group='root', mode='0600'):
    """
    Helper which installs a file with arbitrary permissions and ownership

    This is a replacement for Fabric 1's `put(…, use_sudo=True)` and adds the
    ability to set the expected ownership and permissions in one operation.

    Source: https://github.com/fabric/fabric/issues/1750
    """
    mktemp_result = connection.run('mktemp', hide='out')
    assert mktemp_result.ok

    temp_file = mktemp_result.stdout.strip()

    try:
        connection.put(source, temp_file)
        connection.sudo(f'install -o {owner} -g {group} -m {mode} {temp_file} {dest}')
    finally:
        connection.run(f'rm {temp_file}')


def copy_local_directory_to_remote(c, local_base_path, remote_base_path, to_ignore=[]):
    for root, _, files in os.walk(local_base_path):
        for name in files:
            local_file_path = os.path.join(root, name)
            if not to_ignore or not any([pattern in local_file_path for pattern in to_ignore]):
                remote_file_path =  local_file_path.replace(local_base_path, remote_base_path)
                c.run('mkdir -p {}'.format(os.path.dirname(remote_file_path)))
                c.put(local_file_path, remote_file_path)
                print(local_file_path)


@task
def deploy_elk(ctx):
    # NOTE: this expects that key-based ssh access to elk board is already configured
    # ssh-copy-id - i ~/.ssh/id_rsa.pub mind@source.local

    # Copy all necessary files to rmeote
    with Connection(host=host) as c:
        print('\nDeploying SOURCE to elk board')
        print('*****************************')

        c.run('mkdir -p {}'.format(os.path.join(remote_base_dir, 'local_files')))
        c.run('mkdir -p {}'.format(os.path.join(remote_base_dir, 'sound_usage_log')))

        # Copy UI app files
        print('\n* Copying UI app files...')
        ui_app_remote_dir = os.path.join(remote_base_dir, 'ui_app')
        copy_local_directory_to_remote(c, local_ui_app_path, ui_app_remote_dir, to_ignore=['.pyc', '__pycache__', 'frame', '.idea', 'sound_usage_log', 'recent_queries', 'tokens', 'sm_settings'])

        # Copy sucshi and sensei config files
        print('\n* Copying sushi and sensei config files...') 
        config_files_remote_dir = os.path.join(remote_base_dir, 'config')
        copy_local_directory_to_remote(c, local_config_files_path, config_files_remote_dir, to_ignore=['.service'])

        # Installing python requirements
        print('\n* Installing python requirements...')
        c.run(f'pip3 install -r {ui_app_requirements_remote_path}')

        # Copy VST 2plugin files
        print('\n* Copying plugin files...')
        vst2_plugin_remote_path = os.path.join(remote_base_dir, 'plugin', 'SourceSamplerVST2.so')
        c.run('mkdir -p {}'.format(os.path.dirname(vst2_plugin_remote_path)))
        c.put(local_vst2_elk_binary_path, vst2_plugin_remote_path)
        print(local_vst2_elk_binary_path)

        # Copy and install source, sushi, sensei systemd services
        print('\n* Installing systemd sensei, sushi and source services')
        sudo_install(c, os.path.join(local_config_files_path, "sensei.service"), "/lib/systemd/system/sensei.service", mode='0644')
        sudo_install(c, os.path.join(local_config_files_path, "sushi.service"), "/lib/systemd/system/sushi.service", mode='0644')
        sudo_install(c, os.path.join(local_config_files_path, "source.service"), "/lib/systemd/system/source.service", mode='0644')

        # Reload config files in systemd daemon (so new service files are loaded)
        c.run('sudo systemctl daemon-reload')    

        # Restart 
        print('\n* Restarting sensei, sushi and source services...')
        c.run('sudo systemctl restart sensei')
        time.sleep(1)
        c.run('sudo systemctl restart source')
        time.sleep(1)
        c.run('sudo systemctl restart sushi')


@task
def logs_sushi(ctx):
    with Connection(host=host) as c:
        c.run('sudo journalctl -fu sushi')


@task
def logs_sensei(ctx):
    with Connection(host=host) as c:
        c.run('sudo journalctl -fu sensei')


@task
def logs_source(ctx):
    with Connection(host=host) as c:
        c.run('sudo journalctl -fu source')


@task
def compile_elk(ctx, configuration='Release'):
    print('Cross-compiling Source for ELK platform...')
    print('*********************************************')

    # Generate binary files
    # NOTE: this step is uding using the BinaryBuilder binary compiled in the host platform
    print('\n* Building BinaryData')
    if os.path.exists('SourceSampler/3rdParty/JUCE/extras/BinaryBuilder/Builds/MacOSX/build/Release/BinaryBuilder'):
        os.system("SourceSampler/3rdParty/JUCE/extras/BinaryBuilder/Builds/MacOSX/build/Release/BinaryBuilder SourceSampler/Resources SourceSampler/Source/ BinaryData")
    elif os.path.exists('SourceSampler/3rdParty/JUCE/extras/BinaryBuilder/Builds/LinuxMakefile/build/BinaryBuilder'):
        os.system("SourceSampler/3rdParty/JUCE/extras/BinaryBuilder/Builds/LinuxMakefile/build/BinaryBuilder SourceSampler/Resources SourceSampler/Source/ BinaryData")
    else:
        raise Exception('No BinaryBuilder executable found... fave sure to compile it first running "fab compile-binary-builder')

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
    os.system('docker run --rm -it -v elkvolume:/workdir -v ${PWD}/:/code/source -v ${PWD}/SourceSampler/Builds/ELKAudioOS/build/copied_vst2:/home/sdkuser/.vst -v ${PWD}/SourceSampler/Builds/ELKAudioOS/build/copied_vst3:/home/sdkuser/.vst3 -v ${PWD}/SourceSampler/3rdParty/JUCE:/home/sdkuser/JUCE -v ' + PATH_TO_VST2_SDK_FOR_ELK_CROSS_COMPILATION + ':/code/VST2_SDK -v ${PWD}/elk_platform/build_system/custom-esdk-launch.py:/usr/bin/esdk-launch.py -e CC_CONFIG=' + configuration + ' -e CC_PATH_TO_MAKEFILE=/code/source/SourceSampler/Builds/ELKAudioOS crops/extsdk-container')

    # Undo file replacements
    print('\n* Restoring build files')
    replace_in_file("SourceSampler/JuceLibraryCode/AppConfig.h", replacement=old_appconfig_file_contents)
    replace_in_file("SourceSampler/Builds/ELKAudioOS/Makefile", replacement=old_makefile_file_contents)
    
    print('\nAll done!')


@task
def compile_elk_debug(ctx):
    compile_elk(ctx, configuration='Debug')


def compile_macos(configuration='Release'):
    # Compile release Source for macOS platform
    print('Compiling Source for macOS platform...')
    print('*********************************************\n')
    os.system("cd SourceSampler/Builds/MacOSX/;xcodebuild -configuration {0}".format(configuration))    

    print('\nAll done!')


def compile_projucer_macos():
    print('Compiling Projucer for macOS platform...')
    print('*********************************************\n')
    os.system("cd SourceSampler/3rdParty/JUCE/extras/Projucer/Builds/MacOSX/;xcodebuild -configuration Release GCC_PREPROCESSOR_DEFINITIONS='$GCC_PREPROCESSOR_DEFINITIONS JUCER_ENABLE_GPL_MODE=1' LLVM_LTO=NO")    

    print('\nAll done!')


def compile_binary_builder_macos():
    print('Compiling BinaryBuilder for macOS platform...')
    print('*********************************************\n')
    os.system("cd SourceSampler/3rdParty/JUCE/extras/BinaryBuilder/Builds/MacOSX/;xcodebuild -configuration Release GCC_PREPROCESSOR_DEFINITIONS='$GCC_PREPROCESSOR_DEFINITIONS JUCER_ENABLE_GPL_MODE=1' LLVM_LTO=NO")    

    print('\nAll done!')


def compile_linux(configuration='Release'):
    print('Compiling Source for Linux platform...')
    print('*********************************************\n')

    #Bundle all JS Files
    print('Bundling all JS modules...')
    print('*********************************************\n')
    os.system("cd SourceSampler/3rdParty/StaticBundle; mkdir dist; npx webpack")
    #Beautify bundle file 
    os.system("cd SourceSampler/3rdParty/StaticBundle/dist; js-beautify -f main.js -o mainBF.js")
    #Compile 
    os.system("SourceSampler/3rdParty/JUCE/extras/BinaryBuilder/Builds/LinuxMakefile/build/BinaryBuilder SourceSampler/Resources SourceSampler/Source/ BinaryData")    
    os.system("cd SourceSampler/Builds/LinuxMakefile;make CONFIG={} -j4".format(configuration))    

    print('\nAll done!')


def compile_projucer_linux():
    print('Compiling Projucer for linux platform...')
    print('*********************************************\n')
    os.system("cd SourceSampler/3rdParty/JUCE/extras/Projucer/Builds/LinuxMakefile/;make CONFIG=Release -j4")    

    print('\nAll done!')


def compile_binary_builder_linux():
    print('Compiling BinaryBuilder for linux platform...')
    print('*********************************************\n')
    os.system("cd SourceSampler/3rdParty/JUCE/extras/BinaryBuilder/Builds/LinuxMakefile/;make CONFIG=Release -j4")    

    print('\nAll done!')


@task
def compile(ctx, configuration='Release'):
    if platform.system() == 'Darwin':
        if not os.path.exists('SourceSampler/3rdParty/JUCE/extras/BinaryBuilder/Builds/MacOSX/build/Release/BinaryBuilder'):
            compile_binary_builder_macos()
        compile_macos(configuration=configuration)
    elif platform.system() == 'Linux':
        compile_binary_builder_linux()
        compile_projucer_linux()
        compile_linux(configuration=configuration)
    else:
        raise Exception('Unsupported compilation platform')


@task
def compile_debug(ctx):
    compile(ctx, configuration="Debug")


@task
def compile_binary_builder(ctx):
    if platform.system() == 'Darwin':
        compile_binary_builder_macos()
    elif platform.system() == 'Linux':
        compile_binary_builder_linux()
    else:
        raise Exception('Unsupported compilation platform')


@task
def compile_projucer(ctx):
    if platform.system() == 'Darwin':
        compile_projucer_macos()
    elif platform.system() == 'Linux':
        compile_projucer_linux()
    else:
        raise Exception('Unsupported compilation platform')


@task
def clean(ctx):
    # Remove all intermediate build files for all platforms
    os.system("rm -r SourceSampler/Builds/ELKAudioOS/build/")
    os.system("rm -r SourceSampler/Builds/MacOSX/build/")
    os.system("rm -r SourceSampler/Builds/LinuxMakefile/build/")
