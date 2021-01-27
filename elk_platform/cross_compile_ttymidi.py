import os 

os.system('docker run --rm -it -v elkvolume:/workdir -v ${PWD}/../:/code/source -v ${PWD}/custom-esdk-launch-ttymidi.py:/usr/bin/esdk-launch.py crops/extsdk-container')
