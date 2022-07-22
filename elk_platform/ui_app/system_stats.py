import os
import shutil
import time

from helpers import get_platform, get_command_output

system_stats = {}
last_time_restarting_connman = 0
network_currently_connected = False

last_time_disk_space_checked = 0
last_disk_space_percentage = -1.0
last_disk_usage_bytes = -1.0
last_disk_capacity_bytes = -1.0


def get_disk_stats():
    global last_time_disk_space_checked
    global last_disk_space_percentage
    global last_disk_usage_bytes
    global last_disk_capacity_bytes

    now = time.time()
    if now - last_time_disk_space_checked > 10.0:        
        if get_platform() == "ELK":
            path_to_check = '/udata/'
        else:
            path_to_check = os.path.dirname(os.path.realpath(__file__))
        total, used, free = shutil.disk_usage(__file__)
        last_disk_space_percentage = free/total
        last_disk_usage_bytes = free
        last_disk_capacity_bytes = total
        last_time_disk_space_checked = now
    
    return {
        'disk_free_percentage': last_disk_space_percentage,
        'disk_used_bytes': last_disk_usage_bytes,
        'disk_capacity_bytes': last_disk_capacity_bytes
    }


def collect_system_stats():
    global system_stats
    global last_time_restarting_connman
    global network_currently_connected

    if get_platform() == "ELK":

        # Get system stats like cpu usage, temperature, etc.
        try:
            system_stats.update(get_disk_stats())
            system_stats['temp'] = get_command_output("sudo vcgencmd measure_temp").replace('temp=', '').replace("'C", '')
            system_stats['cpu'] = get_command_output("/bin/grep 'cpu ' /proc/stat | awk '{usage=($2+$4)*100/($2+$4+$5)} END {print usage \"%\"}'")[0:4]
            system_stats['mem'] = get_command_output("free | /bin/grep Mem | awk '{print $3/$2 * 100.0}'")[0:4]
            proc_xenomai_output = get_command_output("/bin/more /proc/xenomai/sched/stat | /bin/grep sushi_b64")
            total_cpu = 0
            for line in proc_xenomai_output.split('\n'):
                total_cpu += float(line.split('sushi_b64')[0].strip().split(' ')[-1])
            system_stats['xenomai_cpu'] = total_cpu
            system_stats['n_sushi_proc'] = get_command_output("/bin/ps -e | /bin/grep sushi | wc -l")

            # Check network status and reconnect if disconnected
            connmanctl_services_output = get_command_output("sudo connmanctl services")
            network_is_connected = '*AO' in connmanctl_services_output
            if network_is_connected:
                if not network_currently_connected:
                    # Network has changed state from not connected to connected
                    # Do some "initialization" stuff that requires a connection
                    # NOTE: nothing to be done so far
                    network_currently_connected = True  # Set this to true so we don't do the initialization again

                system_stats['network_ssid'] = connmanctl_services_output.split('*AO ')[1].split(' ')[0]
                last_time_restarting_connman = 0
            else:
                system_stats['network_ssid'] = '-' + ' (R)' if last_time_restarting_connman != 0 else ''
                if time.time() - last_time_restarting_connman > 10:  # Don't try to reconnect more often than every 10 seconds
                    print('* Restarting connman')
                    last_time_restarting_connman = time.time()
                    get_command_output("sudo connmanctl enable wifi")
                    get_command_output("sudo systemctl restart connman")

            # Check aconnect status and reconnect if disconnected
            aconnect_l_output = get_command_output("aconnect -l")
            aconnect_is_connected = 'Connected' in aconnect_l_output
            if not aconnect_is_connected:
                print('* Running aconnect')
                out = get_command_output("aconnect 16 128")
                print(out)

        except:
            system_stats = {}
    else: 
        system_stats.update(get_disk_stats())
