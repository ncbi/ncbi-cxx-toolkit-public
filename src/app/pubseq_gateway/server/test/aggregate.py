#!/bin/env python3

import os, os.path, sys, json
from sys import maxsize

if len(sys.argv) not in [2, 3]:
    print('Usage: <script.py> <dir name> [aggragate file base name]')
    sys.exit(1)

dirName = os.path.abspath(sys.argv[1])
if not os.path.exists(dirName):
    print('Directory not found: ' + dirName)
    sys.exit(1)

if not os.path.isdir(dirName):
    print(dirName + ' is not a directory')
    sys.exit(1)

dirName = os.path.normpath(dirName) + os.path.sep

def convertToSeconds(s):
    s = s.strip()
    if s.endswith('ms'):
        return float(s[:-2]) / 1000.0
    if s.endswith('us'):
        return float(s[:-2]) / 1000000.0
    if s.endswith('s'):
        return float(s[:-1])
    raise Exception(f'Unknown value to convert to seconds: {s}')

def convertToBytesPerSecond(s):
    s = s.strip()
    if s.endswith('KB/s'):
        return float(s[:-4]) * 1024.0
    if s.endswith('MB/s'):
        return float(s[:-4]) * 1024.0 * 1024.0
    raise Exception(f'Unknown value to convert to bytes per seconds: {s}')

def processFile(content, name, stat):
    stat[name] = {
        'finished': {'time': '', 'req/s': '', 'data': ''},
        'requests': {'total': 0, 'started': 0, 'done': 0, 'succeeded': 0, 'failed': 0, 'errored': 0, 'timeout': 0},
        'status codes': {'2xx': '', '3xx': '', '4xx': '', '5xx': ''},
        'traffic': {'total': '', 'headers': '', 'data': ''},
        'time for request': {'min': '', 'max': '', 'mean': '', 'sd': '', '+/- sd': ''},
        'time for connect': {'min': '', 'max': '', 'mean': '', 'sd': '', '+/- sd': ''},
        'time to 1st byte': {'min': '', 'max': '', 'mean': '', 'sd': '', '+/- sd': ''},
        'req/s': {'min': '', 'max': '', 'mean': '', 'sd': '', '+/- sd': ''}
    }

    for line in content.splitlines():
        line = line.strip()
        if line.startswith('finished in'):
            line = line[len('finished in'):].strip()
            p = line.split(', ')
            stat[name]['finished']['time'] = convertToSeconds(p[0])
            stat[name]['finished']['req/s'] = float(p[1][:-len('req/s')].strip())
            stat[name]['finished']['data'] = convertToBytesPerSecond(p[2])
        elif line.startswith('requests: '):
            line = line.split(':')[1].strip()
            p = line.split(', ')
            stat[name]['requests']['total'] = float(p[0].split(' ')[0])
            stat[name]['requests']['started'] = float(p[1].split(' ')[0])
            stat[name]['requests']['done'] = float(p[2].split(' ')[0])
            stat[name]['requests']['succeeded'] = float(p[3].split(' ')[0])
            stat[name]['requests']['failed'] = float(p[4].split(' ')[0])
            stat[name]['requests']['errored'] = float(p[5].split(' ')[0])
            stat[name]['requests']['timeout'] = float(p[6].split(' ')[0])
        elif line.startswith('status codes:'):
            line = line.split(':')[1].strip()
            p = line.split(', ')
            stat[name]['status codes']['2xx'] = float(p[0].split(' ')[0])
            stat[name]['status codes']['3xx'] = float(p[1].split(' ')[0])
            stat[name]['status codes']['4xx'] = float(p[2].split(' ')[0])
            stat[name]['status codes']['5xx'] = float(p[3].split(' ')[0])
        elif line.startswith('traffic:'):
            line = line.split(':')[1].strip()
            p = line.split(', ')
            total = p[0].split(' ')[1][1:-1]
            stat[name]['traffic']['total'] = float(total)
            headers = p[1].split(' ')[1][1:-1]
            stat[name]['traffic']['headers'] = float(headers)
            data = p[2].split(' ')[1][1:-1]
            stat[name]['traffic']['data'] = float(data)
        elif line.startswith('time for request:'):
            line = line.split(':')[1].strip()
            p = line.split()
            stat[name]['time for request']['min'] = convertToSeconds(p[0])
            stat[name]['time for request']['max'] = convertToSeconds(p[1])
            stat[name]['time for request']['mean'] = convertToSeconds(p[2])
            stat[name]['time for request']['sd'] = convertToSeconds(p[3])
            stat[name]['time for request']['+/- sd'] = float(p[4][:-1])
        elif line.startswith('time for connect:'):
            line = line.split(':')[1].strip()
            p = line.split()
            stat[name]['time for connect']['min'] = convertToSeconds(p[0])
            stat[name]['time for connect']['max'] = convertToSeconds(p[1])
            stat[name]['time for connect']['mean'] = convertToSeconds(p[2])
            stat[name]['time for connect']['sd'] = convertToSeconds(p[3])
            stat[name]['time for connect']['+/- sd'] = float(p[4][:-1])
        elif line.startswith('time to 1st byte:'):
            line = line.split(':')[1].strip()
            p = line.split()
            stat[name]['time to 1st byte']['min'] = convertToSeconds(p[0])
            stat[name]['time to 1st byte']['max'] = convertToSeconds(p[1])
            stat[name]['time to 1st byte']['mean'] = convertToSeconds(p[2])
            stat[name]['time to 1st byte']['sd'] = convertToSeconds(p[3])
            stat[name]['time to 1st byte']['+/- sd'] = float(p[4][:-1])
        elif line.startswith('req/s'):
            line = line.split(':')[1].strip()
            p = line.split()
            stat[name]['req/s']['min'] = float(p[0])
            stat[name]['req/s']['max'] = float(p[1])
            stat[name]['req/s']['mean'] = float(p[2])
            stat[name]['req/s']['sd'] = float(p[3])
            stat[name]['req/s']['+/- sd'] = float(p[4][:-1])

stat = {}
count = 0
for item in os.listdir(dirName):
    if not item.endswith('.out') or not item.startswith('h2load.'):
        continue
    count += 1

    # Processing a file
    content = open(dirName + item, 'r').read()
    processFile(content, item, stat)


# Aggregate some of the values
agg = {
        'time for request': {'min': float(maxsize), 'max': -1.0, 'mean': 0.0},
        'time for connect': {'min': float(maxsize), 'max': -1.0, 'mean': 0.0},
        'time to 1st byte': {'min': float(maxsize), 'max': -1.0, 'mean': 0.0},
        'req/s': {'min': float(maxsize), 'max': -1.0, 'mean': 0.0}
    }
failed = 0
errored = 0
timeout = 0
for name in stat.keys():
    failed += stat[name]['requests']['failed']
    errored += stat[name]['requests']['errored']
    timeout += stat[name]['requests']['timeout']

    for val in agg.keys():
        agg[val]['min'] = min(agg[val]['min'], stat[name][val]['min'])
        agg[val]['max'] = max(agg[val]['max'], stat[name][val]['max'])
        agg[val]['mean'] += stat[name][val]['mean']
count = float(len(stat))
for val in agg.keys():
    agg[val]['mean'] /= count

print(f'Processed {count} files')

if failed + errored + timeout > 0:
    print(f'Failure detected. Failed: {failed} Errored: {errored} Timeout: {timeout}')
    sys.exit(1)

if len(sys.argv) == 3:
    with open(dirName + os.path.basename(sys.argv[2]), 'w') as outfile:
        json.dump(agg, outfile)

print('All OK')
sys.exit(0)

