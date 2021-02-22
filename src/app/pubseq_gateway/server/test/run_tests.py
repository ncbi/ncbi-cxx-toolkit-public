#!/usr/bin/env python3

import sys, os

def toInt(val):
    try:
        return int(val)
    except:
        return None
    return None

params = sys.argv[1:]
if '-h' in params or '--help' in params:
    print('Usage: run_tests.py [-h|--help] [--https] [host|port|host:port]')
    print('Default host: localhost')
    print('Default port: 2180')
    sys.exit(0)

if len(params) not in [0, 1, 2]:
    print('Incorrect number of arguments')
    print('Use the --help option for more information')
    sys.exit(1)

host = None
port = None
https = False

if '--https' in params:
    https = True
    params.remove('--https')

if len(params) not in [0, 1]:
    print('Incorrect arguments')
    print('Use the --help option for more information')
    sys.exit(1)

if len(params) == 1:
    # Three cases: host | port | host:port
    if ':' in params[0]:
        parts = params[0].split(':')
        port = toInt(parts[1])
        if port is None:
            print('port must be an integer')
            sys.exit(1)
        host = parts[0]
    else:
        port = toInt(params[0])
        if port is None:
            host = params[0]


dirname = os.path.abspath(os.path.dirname(sys.argv[0]))
baselineDir = dirname + os.path.sep + 'baseline'
if not os.path.exists(baselineDir):
    # Need to unpack
    baselineArchive = dirname + os.path.sep + 'baseline.tar.gz'
    if not os.path.exists(baselineArchive):
        print('Cannot find baseline archive ' + baselineArchive)
        sys.exit(1)
    if os.system('tar -xzf ' + baselineArchive + ' -C ' + dirname) != 0:
        print('Error unpacking baseline archive ' + baselineArchive)
        sys.exit(1)

server = ''
if host is not None:
    server = host
else:
    server = 'localhost'
if port is not None:
    server += ':' + str(port)
else:
    server += ':2180'

cmd = 'udc --variable PSG_SERVER:' + server + ' --variable PSG_HTTPS:' + str(https) + ' ' + dirname
sys.exit(os.system(cmd))

