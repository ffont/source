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
            ("../server/templates/index.html", remote_dir + 'templates/'),
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

    # Cross-compile Source
    print('Coss-compiling Source for ELK platform...')
    print('*********************************************\n')
    os.system("find ../SourceSampler/Builds/ELKAudioOS/build/intermediate/Release/ -type f \( \! -name 'include_*' \) -exec rm {} \;")
    os.system('docker run --rm -it -v elkvolume:/workdir -v ${PWD}/../:/code/source -v ${PWD}/../SourceSampler/3rdParty/JUCE_ELK:/home/sdkuser/JUCE -v ${PWD}/../../VST_SDK/VST2_SDK:/code/VST2_SDK -v ${PWD}/custom-esdk-launch.py:/usr/bin/esdk-launch.py crops/extsdk-container')

    print('\nAll done!')


@task
def compile_macos(ctx):

    # Compile release Source for macOS platform
    print('Compiling Source for macOS platform...')
    print('*********************************************\n')
    os.system("cd ../SourceSampler/Builds/MacOSX/;xcodebuild -configuration Release GCC_PREPROCESSOR_DEFINITIONS='$GCC_PREPROCESSOR_DEFINITIONS JUCER_ENABLE_GPL_MODE=1' LLVM_LTO=NO")    

    print('\nAll done!')


@task
def compile(ctx):
    compile_macos(ctx)
    compile_elk(ctx)
    
    
@task
def deploy_elk(ctx):
    compile_elk(ctx)
    send_elk(ctx)
