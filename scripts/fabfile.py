import os

from fabric import task, Connection

host = 'mind@elk-pi.local'
remote_dir = '/home/mind/'

@task
def clean(ctx):

    # Remove all intermediate build files (for ELK build)
    os.system("rm -r ../SourceSampler/Builds/ELKAudioOS/build/")


@task
def send(ctx):

    # Copy compiled file and sushi configuration to board
    print('\nSending compiled SourceSamler and config files to board...')
    print('********************************************************\n')
    with Connection(host=host, connect_kwargs={'password': 'elk'}) as c:
        for local_file, destination_dir in [
            ("../start.sh", remote_dir),
            ("../source_sushi_config.json", remote_dir),
            ("../SourceSampler/Builds/ELKAudioOS/build/SourceSampler.so", remote_dir)
        ]:
            print('- Copying {0} to {1}'.format(local_file, destination_dir))
            c.put(local_file, destination_dir)

    print('\nAll done!')
    print('You can now run OctopushOS on the ELK board with the command:')
    print('./start.sh')
    print('\n')


@task
def compile(ctx):

    # Cross-compile OctopushOS
    print('Coss-compiling SourceSampler for ELK platform...')
    print('*********************************************\n')
    os.system("find ../SourceSampler/Builds/ELKAudioOS/build/intermediate/Release/ -type f \( \! -name 'include_*' \) -exec rm {} \;")
    os.system('docker run --rm -it -v elkvolume:/workdir -v ${PWD}/../:/code/source -v ${PWD}/../../JUCE_ELK:/home/sdkuser/JUCE -v ${PWD}/../../VST_SDK/VST2_SDK:/code/VST2_SDK -v ${PWD}/custom-esdk-launch.py:/usr/bin/esdk-launch.py crops/extsdk-container')

    print('\nAll done!')


@task
def deploy(ctx):
    compile(ctx)
    send(ctx)
