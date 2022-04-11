import os

# Cross-compile the ttymidi program used to use DIN5 MIDI ports in Blackboard board
print('Corss-compiling mod-ttymidi for ELK platform...')
print('***********************************************\n')

print('\n* Cross-compiling')
os.system('docker run --rm -it -v elkvolume:/workdir -v ${PWD}/../../:/code/source -v ${PWD}/custom-esdk-launch-ttymidi.py:/usr/bin/esdk-launch.py crops/extsdk-container')
