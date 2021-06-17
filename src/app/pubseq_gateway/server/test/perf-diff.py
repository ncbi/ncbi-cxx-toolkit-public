#!/bin/env python3

import sys, os.path, json, math


def round_half_up(n, decimals=0):
    multiplier = 10 ** decimals
    return math.floor(n*multiplier + 0.5) / multiplier

def convertToSeconds(v):
    if v >= 1.0:
        return str(round_half_up(v, 1)) + 's'
    v = v * 1000
    if v >= 1.0:
        return str(int(round_half_up(v, 0))) + 'ms'
    v = v * 1000
    return str(int(round_half_up(v, 0))) + 'us'


if len(sys.argv) != 2:
    print('Usage: ' + os.path.basename(sys.argv[0]) + ' <perf dirname>')
    sys.exit(1)

dirName = os.path.abspath(sys.argv[1])
if not os.path.exists(dirName):
    print('Directory not found: ' + dirName)
    sys.exit(1)

if not os.path.isdir(dirName):
    print(dirName + ' is not a directory')
    sys.exit(1)

dirName = os.path.normpath(dirName) + os.path.sep

# aggr.resolve-primary-cache.new.json
# i.e. <arrg.>NAME<.suffix><.json>
# so the dict will have: NAME -> {suffix1: fname1, suffix2: fname2}
files = {}
suffixes = set()
for item in os.listdir(dirName):
    if not item.startswith('aggr.') or not item.endswith('.json'):
        continue
    name = item[5:-5]
    p = name.split('.')
    if len(p) != 2:
        print('Unexpected format of the aggregated JSON file: ' + name)
        sys.exit(1)

    name = p[0]
    suffix = p[1]

    suffixes.add(suffix)
    if name in files:
        files[name][suffix] = dirName + item
    else:
        files[name] = { suffix: dirName + item }

if len(files) == 0:
    print('Nothing to compare')
    sys.exit(0)

# Sanity checks
if len(suffixes) not in [1, 2]:
    print('Unexpected number of suffixes found')
    print(suffixes)
    sys.exit(1)

if len(suffixes) == 1:
    print('One suffix is found. Run once more with another server and another suffix')
    sys.exit(0)

# Must be two suffixes for each file
for name in files.keys():
    if len(files[name]) != 2:
        print('Unexpected number of files for the name ' + name)
        print(files[name])
        sys.exit(1)

# All good, each name has 2 items
suffixes = list(suffixes)
suffixes.sort(reverse=True)
suff1 = suffixes.pop(0)
suff2 = suffixes.pop(0)

print('Legend:')
print('O - ' + suff1)
print('N - ' + suff2)
print()

names = list(files.keys())
names.sort()
for name in names:
    firstFile = files[name][suff1]
    secondFile = files[name][suff2]

    with open(firstFile) as jsonfile:
        data1 = json.load(jsonfile)
    with open(secondFile) as jsonfile:
        data2 = json.load(jsonfile)

    formatStr = '{0: <20} {1: >10} {2: >10} {3: >10} {4: >10} {5: >10} {6: >10} {7: >10} {8: >10} {9: >10} '
    print('Case: ' + name)
    print(formatStr.format(' ', 'min(N/O)', 'max(N/O)', 'mean(N/O)',
                                'min(O)', 'max(O)', 'mean(O)', 'min(N)', 'max(N)', 'mean(N)'))
    # compare taking first suffix as 100%
    for param in data1.keys():
        minN = data2[param]['min']
        maxN = data2[param]['max']
        meanN = data2[param]['mean']
        minO = data1[param]['min']
        maxO = data1[param]['max']
        meanO = data1[param]['mean']

        minDiff = ((minN / minO) - 1.0) * 100   # percent
        minDiff = str(round_half_up(minDiff, 1)) + '%'

        maxDiff = ((maxN / maxO) - 1.0) * 100   # percent
        maxDiff = str(round_half_up(maxDiff, 1)) + '%'

        meanDiff = ((meanN / meanO) - 1.0) * 100   # percent
        meanDiff = str(round_half_up(meanDiff, 1)) + '%'


        if param == 'req/s':
            print(formatStr.format(param, minDiff, maxDiff, meanDiff,
                                   round_half_up(minO, 1), round_half_up(maxO, 1), round_half_up(meanO, 1),
                                   round_half_up(minN, 1), round_half_up(maxN, 1), round_half_up(meanN, 1)))
        else:
            print(formatStr.format(param, minDiff, maxDiff, meanDiff,
                                   convertToSeconds(minO), convertToSeconds(maxO), convertToSeconds(meanO),
                                   convertToSeconds(minN), convertToSeconds(maxN), convertToSeconds(meanN)))
    print()
