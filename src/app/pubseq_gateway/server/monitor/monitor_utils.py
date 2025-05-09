"""Some common functionality used by monitor and analyze scripts"""

# pylint: disable=C0301     # too long lines
# pylint: disable=C0103     # var names
# pylint: disable=C0305     # trailing newlines
# pylint: disable=C0116     # docstring
# pylint: disable=C0114     # module docstring
# pylint: disable=C0410     # multiple imports in one line
# pylint: disable=W0703     # too general exception
# pylint: disable=W0702     # No exception type(s) specified (bare-except)
# pylint: disable=W0212     # Access to a protected member
# pylint: disable=R1702     # Too many nested blocks
# pylint: disable=R0912     # Too many branches
# pylint: disable=C0206     # Consider iterating with .items()
# pylint: disable=R1732     # Consider using 'with' for resource-allocating operations
# pylint: disable=C0201     # Consider iterating the dictionary directly instead of calling .keys()
# pylint: disable=R0911     # Too many return statements
# pylint: disable=W0603     # Using the global statement
# pylint: disable=W0602     # Using global for
# pylint: disable=R0913     # Too many arguments
# pylint: disable=R0917     # Too many positional arguments
# pylint: disable=R0915     # Too many statements
# pylint: disable=R0914     # Too many local variables


import datetime
import sys
import os, os.path
from ncbi.grid.deployment_info import services


def timestamp():
    now = datetime.datetime.now()
    return now.strftime('%Y-%m-%d %H:%M:%S')


def print_error(msg):
    print(timestamp() + ' ' + msg, file=sys.stderr)


def isInt(val):
    try:
        _ = int(val)
    except:
        return False
    return True

def isFloat(val):
    try:
        _ = float(val)
    except:
        return False
    return True

def isHostPortServiceDirectory(dst_dir, dir_item):
    """Format: <host:port>[-<service>]"""
    if dir_item in ['.', '..']:
        return False
    if not os.path.isdir(dst_dir + dir_item):
        return False
    if not ':' in dir_item:
        return False
    parts = dir_item.split(':')
    if len(parts) != 2:
        return False
    host = parts[0]

    parts = parts[1].split('-')
    if len(parts) not in [1, 2]:
        return False
    if not isInt(parts[0]):
        return False

    if len(parts) == 1:
        return (host, int(parts[0]), '')
    return (host, int(parts[0]), parts[1])


def isTimestampDirectory(kind_dir, kind_item):
    """Format: YYYYMMDD-hh"""
    if kind_item in ['.', '..']:
        return False
    if not os.path.isdir(kind_dir + kind_item):
        return False
    # Check that the directory name conforms to
    # what was created
    parts = kind_item.split('-')
    if len(parts) != 2:
        return False
    if not isInt(parts[0]) or not isInt(parts[1]):
        return False
    # convert into timestamp
    dir_ts = datetime.datetime.strptime(kind_item, '%Y%m%d-%H')
    return dir_ts


def isJsonFile(ts_dir, ts_item, start_timestamp, end_timestamp, print_verbose_function):
    if ts_item in ['.', '..']:
        return False
    if not os.path.isfile(ts_dir + ts_item):
        return False
    parts = ts_item.split('-')
    if len(parts) != 4:
        return False
    try:
        # Format: YYYYMMDD-hh-mm-ss.ms
        name_timestamp = datetime.datetime.strptime(ts_item, '%Y%m%d-%H-%M-%S.%f')
    except Exception:
        print_verbose_function(f"The file {ts_dir}{ts_item} name is not conforming the timestamp. Skipping.")
        return False

    # Try to read the beginning and see if it is not an error/message file
    try:
        with open(ts_dir + ts_item, "r", encoding='utf-8') as f:
            partial_content = f.read(128)
    except Exception as exc:
        print_verbose_function(f"Error reading file {ts_dir}{ts_item}. Skipping.\n" + str(exc))
        return False

    if partial_content.startswith('ERROR'):
        print_verbose_function(f"Skipping file {ts_dir}{ts_item} because it is an error file")
        return False
    if partial_content.startswith('MESSAGE'):
        print_verbose_function(f"Skipping file {ts_dir}{ts_item} because it is a message file")
        return False

    if start_timestamp is not None and end_timestamp is not None:
        # Check the file timestamp
        if name_timestamp < start_timestamp:
            print_verbose_function(f"Skipping file {ts_dir}{ts_item} because its timestamp is before the start timestamp")
            return False
        if name_timestamp > end_timestamp:
            print_verbose_function(f"Skipping file {ts_dir}{ts_item} because its timestamp is after the end timestamp")
            return False
    return True


def buildPSGInstances(in_services, print_verbose_func):
    """ Resolve services if needed; exclude overlapping if so"""

    # Result is a dictionary <service> -> [ tuples(host,port) ]
    # Individually provided instances will have '' service
    result = {}
    found_services = []
    for item in in_services:
        if ':' in item:
            # supposedly host:port
            parts = item.split(':')
            if len(parts) != 2:
                print_error(f'The server "{item}" does not follow host:port spec')
                return None
            if not isInt(parts[1]):
                print_error(f'The server "{item}" port is not integer')
                return None
            if '' not in result:
                result[''] = []
            addr = (parts[0], int(parts[1]))
            if addr in result['']:
                print_verbose_func(f'The {item} is provided twice. Ignore and continue')
            else:
                result[''].append(addr)
        else:
            # supposedly a service name
            if item.upper() not in result:
                found_services.append(item.upper())
                result[item.upper()] = []
                deploymentServices = services.get_services()
                for svc in deploymentServices[0]:
                    if svc.name.upper() == item.upper():
                        result[item.upper()].append((svc.host, int(svc.port)))

    print_verbose_func(f'Collected psg instances: {result}')
    print_verbose_func('Sanitizing psg instances...')

    to_remove = []
    if '' in result:
        for instance in result['']:
            for svc_name in found_services:
                for item in result[svc_name]:
                    no_domain1 = item[0].split('.')[0].lower()
                    no_domain2 = instance[0].split('.')[0].lower()
                    if no_domain1 == no_domain2:        # host match
                        if item[1] == instance[1]:      # port match
                            if instance not in to_remove:
                                to_remove.append(instance)
                                print_verbose_func(f'Removing host {instance[0]}:{instance[1]} because it is a member of service {svc_name}')
                                break

    svc_to_remove = []
    for svc_name in found_services:
        if len(result[svc_name]) == 0:
            svc_to_remove.append(svc_name)
            print_verbose_func(f'Removing service {svc_name} because there were no PSG instances in this service')

    if svc_to_remove:
        for svc_name in svc_to_remove:
            del result[svc_name]

    if to_remove:
        for item in to_remove:
            result[''].remove(item)
    else:
        print_verbose_func('No overlapping detected')

    total = 0
    for svc_name in result.keys():
        total += len(result[svc_name])
    if total == 0:
        print_error('No psg instances were identified')
        return None

    return result

