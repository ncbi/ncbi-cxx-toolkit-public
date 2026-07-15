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
    print('Usage: run_tests.py [-h|--help] [--https] [--hup-cookie cookie] [host|port|host:port]')
    print('Default host: localhost')
    print('Default port: 2180')
    sys.exit(0)

host = None
port = None
https = False
hup_cookie = None

if '--https' in params:
    https = True
    params.remove('--https')

for index in range(len(params)):
    if params[index] == '--hup-cookie':
        if index == len(params) - 1:
            print('Incorrect arguments')
            print('Use the --help option for more information')
            sys.exit(1)
        hup_cookie = params[index + 1]
        del params[index]   # --hup-cookie
        del params[index]   # cookie

        # the cookie typically comes from a web interface so some characters
        # need to be replaced
        hup_cookie = hup_cookie.replace("%3D", "=")
        hup_cookie = hup_cookie.replace("%3B", ";")
        hup_cookie = hup_cookie.replace("%40", "@")
        hup_cookie = hup_cookie.replace("%2540ncbi.nlm.nih.gov", "")
        break

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

if not hup_cookie:
    print('HUP cookie is not provided. The HUP tests will fail.')


cmd = 'udc --variable PSG_SERVER:' + server + ' --variable PSG_HTTPS:' + str(https) + ' ' + dirname + '/psg.robot'
if hup_cookie:
    cmd = 'HUP_COOKIE="' + hup_cookie + '" ' + cmd
retCode = os.system(cmd)
if retCode != 0:
    low = retCode & 0xFF
    hi = (retCode >> 8) & 0xFF
    if low == 0:
        retCode = hi
sys.exit(retCode)

