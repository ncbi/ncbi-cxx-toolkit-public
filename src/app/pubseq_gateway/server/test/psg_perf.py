#!/usr/bin/env python3

import sys, os, os.path, time, json, math, statistics, datetime
from optparse import OptionParser
from urllib import request as urlrequest


def round_half_up(n, decimals=0):
    multiplier = 10 ** decimals
    return math.floor(n*multiplier + 0.5) / multiplier

def convertToSeconds(v):
    # Use ms only
    return str(round_half_up(v * 1000, 1)) + 'ms'


    #if v >= 1.0:
    #    return str(round_half_up(v, 1)) + 's'
    #v = v * 1000
    #if v >= 1.0:
    #    return str(int(round_half_up(v, 0))) + 'ms'
    #v = v * 1000
    #return str(int(round_half_up(v, 0))) + 'us'

def inverse(sign):
    if sign == '-':
        return '+'
    return '-'

def getVersion(address):
    try:
        url = 'http://' + address + '/ADMIN/info'
        req = urlrequest.Request(url)
        response = urlrequest.urlopen(req, timeout=5)
        content = response.read().decode('utf-8')

        info = json.loads(content)
        return info['Version']
    except:
        return 'unknown'

def collectRun(verbose, runResults):

    if verbose:
        print('Collecting results')

    dirName = os.path.dirname(__file__) + os.path.sep + 'perf'
    dirName = os.path.normpath(dirName) + os.path.sep

    if not os.path.exists(dirName):
        print('Directory not found: ' + dirName)
        sys.exit(1)
    if not os.path.isdir(dirName):
        print(dirName + ' is not a directory')
        sys.exit(1)

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
        print('No files found to collect h2load run results')
        sys.exit(0)

    # Sanity checks
    if len(suffixes) != 2:
        print('Unexpected number of suffixes found in ' + dirName)
        print(suffixes)
        sys.exit(1)

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


    names = list(files.keys())
    names.sort()
    for name in names:
        firstFile = files[name][suff1]
        secondFile = files[name][suff2]

        with open(firstFile) as jsonfile:
            data1 = json.load(jsonfile)
        with open(secondFile) as jsonfile:
            data2 = json.load(jsonfile)

        if name not in runResults:
            runResults[name] = {}
        if suff1 not in runResults[name]:
            runResults[name][suff1] = []
        if suff2 not in runResults[name]:
            runResults[name][suff2] = []

        runResults[name][suff1].append(data1)
        runResults[name][suff2].append(data2)

    os.system('rm -rf ' + dirName + '*.json')


def getMetricNames(oneRun):
    # Metric names are the same for each run
    return list(oneRun.keys())

def getValueNames(oneRun):
    # Values are the same for each metric for each run
    for key in oneRun:
        return list(oneRun[key].keys())

def getNames(verbose, runResults):
    # get metric names and value names
    for caseName in runResults:
        for serverName in runResults[caseName]:
            metricNames = getMetricNames(runResults[caseName][serverName][0])
            valueNames = getValueNames(runResults[caseName][serverName][0])
            break
    if verbose:
        print('Metric names: ' + str(metricNames))
        print('Value names: ' + str(valueNames))
    return metricNames, valueNames

def preprocessData(verbose, metricNames, valueNames, runResults, throws):

    for caseName in runResults:
        for serverName in runResults[caseName]:

            accum = {}
            for mName in metricNames:
                accum[mName] = {}
                for vName in valueNames:
                    accum[mName][vName] = []        # list of run values

            for oneRun in runResults[caseName][serverName]:
                for mName in metricNames:
                    for vName in valueNames:
                        accum[mName][vName].append(oneRun[mName][vName])

            for mName in metricNames:
                for vName in valueNames:
                    # make a copy

                    valuesCopy = accum[mName][vName].copy()
                    valuesCopy.sort()
                    if throws > 0:
                        lastIndex = len(accum[mName][vName]) - 1
                        for x in range(throws):
                            accum[mName][vName].remove(valuesCopy[x])
                            accum[mName][vName].remove(valuesCopy[lastIndex - x])

            runResults[caseName][serverName] = accum

    # Add artificial servers
    for caseName in runResults:
        runResults[caseName]['T1E'] = {}
        runResults[caseName]['T1O'] = {}
        runResults[caseName]['T2E'] = {}
        runResults[caseName]['T2O'] = {}
        runResults[caseName]['T12E1'] = {}
        runResults[caseName]['T12E2'] = {}

        for mName in metricNames:
            runResults[caseName]['T1E'][mName] = {}
            runResults[caseName]['T1O'][mName] = {}
            runResults[caseName]['T2E'][mName] = {}
            runResults[caseName]['T2O'][mName] = {}
            runResults[caseName]['T12E1'][mName] = {}
            runResults[caseName]['T12E2'][mName] = {}
            for vName in valueNames:
                runResults[caseName]['T1E'][mName][vName] = []
                runResults[caseName]['T1O'][mName][vName] = []
                runResults[caseName]['T2E'][mName][vName] = []
                runResults[caseName]['T2O'][mName][vName] = []
                runResults[caseName]['T12E1'][mName][vName] = []
                runResults[caseName]['T12E2'][mName][vName] = []

            evenValCount = int(len(runResults[caseName]['old'][mName][vName]) / 2)
            for vName in valueNames:
                for x in range(evenValCount):
                    evenIndex = 2 * x
                    oddIndex = evenIndex + 1
                    runResults[caseName]['T1E'][mName][vName].append(runResults[caseName]['old'][mName][vName][evenIndex])
                    runResults[caseName]['T1O'][mName][vName].append(runResults[caseName]['old'][mName][vName][oddIndex])
                    runResults[caseName]['T2E'][mName][vName].append(runResults[caseName]['new'][mName][vName][evenIndex])
                    runResults[caseName]['T2O'][mName][vName].append(runResults[caseName]['new'][mName][vName][oddIndex])
                    runResults[caseName]['T12E1'][mName][vName].append(runResults[caseName]['old'][mName][vName][evenIndex])
                    runResults[caseName]['T12E2'][mName][vName].append(runResults[caseName]['new'][mName][vName][evenIndex])


def aggregate(verbose, metricNames, valueNames, runResults):

    for caseName in runResults:
        for serverName in runResults[caseName]:
            for mName in metricNames:
                for vName in valueNames:
                    valueCount = len(runResults[caseName][serverName][mName][vName])
                    total = sum(runResults[caseName][serverName][mName][vName])
                    runResults[caseName][serverName][mName][vName + 'Average'] = float(total) / float(valueCount)
                    runResults[caseName][serverName][mName][vName + 'StandardDeviation'] = statistics.stdev(runResults[caseName][serverName][mName][vName])
                    runResults[caseName][serverName][mName][vName + 'Mean'] = statistics.mean(runResults[caseName][serverName][mName][vName])


def printOnePair(verbose, metricNames, valueNames, runResults, alertThreshold, first, second, server1, server2, versionMap, runs):

    formatStr = '{0: <50} {1: >10} {2: >10} {3: >10} {4: >10} {5: >10} {6: <6}'

    # print the results
    subst = { 'old':   {'prefix': '',      'server1': 'Server 1', 'server2': 'Server 2'},
              'T1E':   {'prefix': 'T1: ',  'server1': 'Server 1', 'server2': 'Server 1'},
              'T2E':   {'prefix': 'T2: ',  'server1': 'Server 2', 'server2': 'Server 2'},
              'T12E1': {'prefix': 'T12: ', 'server1': 'Server 1', 'server2': 'Server 2'}
            }

    print()
    print(subst[first]['prefix'] + subst[first]['server1'] + ' (' + server1 + ', v.' + versionMap[first] +
          ') vs ' + subst[first]['server2'] + ' (' + server2 + ', v.' + versionMap[second] +
          ') [' + str(runs) + ' runs]')
    print("'+' means that the " + subst[first]['server2'] + " looks better than the " + subst[first]['server1'])
    print("'-' means that the " + subst[first]['server2'] + " looks worse than the " + subst[first]['server1'])
    print()
    print(formatStr.format('Metric', subst[first]['server1'], subst[first]['server2'], 'delta-%  ',
                           'StdDev1', 'StdDev2', 'Alert'))

    for caseName in runResults:
        for mName in metricNames:
            for vName in valueNames:
                oldValAvg = runResults[caseName][first][mName][vName + 'Average']
                newValAvg = runResults[caseName][second][mName][vName + 'Average']

                maxMean = float(max(runResults[caseName][first][mName][vName + 'Mean'],
                                    runResults[caseName][second][mName][vName + 'Mean']))
                oldStdDevPercent = float(runResults[caseName][first][mName][vName + 'StandardDeviation']) / maxMean * 100
                oldStdDevPercent = str(int(round_half_up(oldStdDevPercent, 0))) + '%'
                newStdDevPercent = float(runResults[caseName][second][mName][vName + 'StandardDeviation']) / maxMean * 100
                newStdDevPercent = str(int(round_half_up(newStdDevPercent, 0))) + '%'

                # Memorize for the use in the summary
                runResults[caseName][first][mName][vName + 'StandardDeviationPercent'] = oldStdDevPercent
                runResults[caseName][second][mName][vName + 'StandardDeviationPercent'] = newStdDevPercent

                if mName == 'req/s':
                    # larger is better
                    sign = '+'
                else:
                    sign = '-'

                alert = ''
                if oldValAvg < newValAvg:
                    percent = (float(oldValAvg) / float(newValAvg)) * 100
                else:
                    percent = (float(newValAvg) / float(oldValAvg)) * 100
                    sign = inverse(sign)

                if 100.0 - percent > alertThreshold:
                    alert = 'ALERT'
                    if vName != 'min' and vName != 'max':
                        alert += '!'
                percent = 100 - int(round_half_up(percent, 0))
                if percent == 0:
                    sign = '='
                percent = str(percent) + '%'

                # Memorize for the use in the summary
                runResults[caseName][first][mName][vName + 'ValuePercent'] = percent
                runResults[caseName][second][mName][vName + 'ValuePercent'] = percent
                runResults[caseName][first][mName][vName + 'ValuePercentSign'] = sign
                runResults[caseName][second][mName][vName + 'ValuePercentSign'] = sign

                percent += ' ' + sign

                fullName = caseName + ' (' + mName + '[' + vName + ']):'
                if mName == 'req/s':
                    print(formatStr.format(fullName, round_half_up(oldValAvg, 1), round_half_up(newValAvg, 1),
                                           percent, oldStdDevPercent, newStdDevPercent, alert))
                else:
                    print(formatStr.format(fullName, convertToSeconds(oldValAvg), convertToSeconds(newValAvg),
                                           percent, oldStdDevPercent, newStdDevPercent, alert))


def printSummary(verbose, metricNames, valueNames, runResults, alertThreshold, server1, server2, versionMap, runs):

    formatStr = '{0: <50} {1: >9} {2: >9} {3: >9} {4: >7} {5: >7} {6: <6} | {7: >6} {8: >7} {9: >7} | {10: >5} {11: >6} {12: >6} | {13: >5} {14: >6} {15: >6} |'

    # print the results

    first = 'old'
    second = 'new'
    print()
    print('Summary: Server 1 (' + server1 + ', v.' + versionMap[first] + ') vs Server 2 (' + server2 + ', v.' + versionMap[second] + ') [' + str(runs) + ' runs]')
    print("'+' means that the Server 2 looks better than the Server 1")
    print("'-' means that the Server 2 looks worse than the Server 1")
    print()
    print(formatStr.format('Metric', 'Server 1', 'Server 2', 'delta-%  ', 'StdDev1', 'StdDev2', 'Alert',
                           'T12-d%', 'T12-SD1', 'T12-SD2',
                           'T1-d%',  'T1-SD1',  'T1-SD2',
                           'T2-d%',  'T2-SD1',  'T2-SD2'))

    for caseName in runResults:
        for mName in metricNames:
            for vName in valueNames:
                oldValAvg = runResults[caseName][first][mName][vName + 'Average']
                newValAvg = runResults[caseName][second][mName][vName + 'Average']

                maxMean = float(max(runResults[caseName][first][mName][vName + 'Mean'],
                                    runResults[caseName][second][mName][vName + 'Mean']))
                oldStdDevPercent = float(runResults[caseName][first][mName][vName + 'StandardDeviation']) / maxMean * 100
                oldStdDevPercent = str(int(round_half_up(oldStdDevPercent, 0))) + '%'
                newStdDevPercent = float(runResults[caseName][second][mName][vName + 'StandardDeviation']) / maxMean * 100
                newStdDevPercent = str(int(round_half_up(newStdDevPercent, 0))) + '%'

                if mName == 'req/s':
                    # larger is better
                    sign = '+'
                else:
                    sign = '-'

                alert = ''
                if oldValAvg < newValAvg:
                    percent = (float(oldValAvg) / float(newValAvg)) * 100
                else:
                    percent = (float(newValAvg) / float(oldValAvg)) * 100
                    sign = inverse(sign)

                if 100.0 - percent > alertThreshold:
                    alert = 'ALERT'
                    if vName != 'min' and vName != 'max':
                        alert += '!'
                percent = 100 - int(round_half_up(percent, 0))
                if percent == 0:
                    sign = '='
                percent = str(percent) + '%' + ' ' + sign

                fullName = caseName + ' (' + mName + '[' + vName + ']):'
                if mName == 'req/s':
                    print(formatStr.format(fullName, round_half_up(oldValAvg, 1), round_half_up(newValAvg, 1),
                                           percent, oldStdDevPercent, newStdDevPercent, alert,
                                           runResults[caseName]['T12E1'][mName][vName + 'ValuePercent'] + ' ' + runResults[caseName]['T12E1'][mName][vName + 'ValuePercentSign'],
                                           runResults[caseName]['T12E1'][mName][vName + 'StandardDeviationPercent'],
                                           runResults[caseName]['T12E2'][mName][vName + 'StandardDeviationPercent'],
                                           runResults[caseName]['T1E'][mName][vName + 'ValuePercent'],
                                           runResults[caseName]['T1E'][mName][vName + 'StandardDeviationPercent'],
                                           runResults[caseName]['T1O'][mName][vName + 'StandardDeviationPercent'],
                                           runResults[caseName]['T2E'][mName][vName + 'ValuePercent'],
                                           runResults[caseName]['T2E'][mName][vName + 'StandardDeviationPercent'],
                                           runResults[caseName]['T2O'][mName][vName + 'StandardDeviationPercent']
                                           ))
                else:
                    print(formatStr.format(fullName, convertToSeconds(oldValAvg), convertToSeconds(newValAvg),
                                           percent, oldStdDevPercent, newStdDevPercent, alert,
                                           runResults[caseName]['T12E1'][mName][vName + 'ValuePercent'] + ' ' + runResults[caseName]['T12E1'][mName][vName + 'ValuePercentSign'],
                                           runResults[caseName]['T12E1'][mName][vName + 'StandardDeviationPercent'],
                                           runResults[caseName]['T12E2'][mName][vName + 'StandardDeviationPercent'],
                                           runResults[caseName]['T1E'][mName][vName + 'ValuePercent'],
                                           runResults[caseName]['T1E'][mName][vName + 'StandardDeviationPercent'],
                                           runResults[caseName]['T1O'][mName][vName + 'StandardDeviationPercent'],
                                           runResults[caseName]['T2E'][mName][vName + 'ValuePercent'],
                                           runResults[caseName]['T2E'][mName][vName + 'StandardDeviationPercent'],
                                           runResults[caseName]['T2O'][mName][vName + 'StandardDeviationPercent']
                                           ))


def main():

    parser = OptionParser(
    """
    %prog <old server> <new server> [options]
    """)

    parser.add_option("-v", "--verbose",
                      action="store_true", dest="verbose", default=False,
                      help="be verbose (default: False)")
    parser.add_option("--https",
                      action="store_true", dest="https", default=False,
                      help="Use https (default: False)")
    parser.add_option("--runs",
                      dest="runs", default="10",
                      help="The number of runs against each server")
    parser.add_option("--delay",
                      dest="delay", default="1",
                      help="Delay between runs in minutes")
    parser.add_option("--throws",
                      dest="throws", default="2",
                      help="Number of worst/best run results to throw away")
    parser.add_option("--h2load-count",
                      dest="h2loadcount", default="10",
                      help="Number of h2load utility instances")
    parser.add_option("--alert-threshold",
                      dest="alertThreshold", default="10",
                      help="Percentage difference threshold to print ALERT")


    options, args = parser.parse_args()
    if len(args) != 2:
        print('bad arguments')
        return 1

    oldServer = args[0]
    newServer = args[1]

    runs = int(options.runs)
    delay = int(options.delay)
    throws = int(options.throws)
    h2loadcount = int(options.h2loadcount)
    https = options.https
    verbose = options.verbose
    alertThreshold = float(options.alertThreshold)

    if runs - 2 * throws < 4:
        print('Bad arguments: not enough total runs after throwing worst/best')
        return 1

    # { 'case 1 name': 'old': [json1, json2, ...],
    #                  'new': [json1, json2, ...],
    #   'case 2 name': 'old': [...]
    #   ...
    # }
    runResults = {}

    for x in range(runs):
        now = datetime.datetime.now()
        print(now.strftime('%Y-%m-%d %H:%M:%S') + ' Run #' + str(x) + ' ...', flush=True)
        cmd = ' '.join(['./performance.sh',
                        '--server1', oldServer,
                        '--server2', newServer,
                        '--h2load-count', str(h2loadcount)])
        if https:
            cmd += ' --https'
        if verbose:
            print('Running command: ' + cmd)
        retCode = os.system(cmd)
        if verbose:
            print('Finished with status: ' + str(retCode))

        collectRun(verbose, runResults)
        if x != runs - 1:
            time.sleep(delay * 60)

    if verbose:
        print('Collected raw results:')
        print(runResults)

    metricNames, valueNames = getNames(verbose, runResults)
    preprocessData(verbose, metricNames, valueNames, runResults, throws)

    # Here the runResults are as follows:
    # { 'case 1 name': 'old': 'metric 1': { 'min': [0.55], 'max': [0.66] }
    #                         'metric 2': { 'min': [11.22], 'max': [13.0]}
    #                  'new': 'metric 1': { 'min': [0.56], 'max': [0.72]}
    #                         'metric 2': { 'min': [12.13], 'max': [14.9]},
    #                  'T1E': 'metric 1': { 'min': [0.56], 'max': [0.72]}
    #                         'metric 2': { 'min': [12.13], 'max': [14.9]},
    #                  'T1O': 'metric 1': { 'min': [0.56], 'max': [0.72]}
    #                         'metric 2': { 'min': [12.13], 'max': [14.9]},
    #   ...
    # }

    aggregate(verbose, metricNames, valueNames, runResults)

    versionMap = { 'old': getVersion(oldServer) }
    if oldServer == newServer:
        versionMap['new'] = versionMap['old']
    else:
        versionMap['new'] = getVersion(newServer)
    versionMap['T1E'] = versionMap['old']
    versionMap['T1O'] = versionMap['old']
    versionMap['T2E'] = versionMap['new']
    versionMap['T2O'] = versionMap['new']
    versionMap['T12E1'] = versionMap['old']
    versionMap['T12E2'] = versionMap['new']

    evenRuns = int((runs - 2 * throws) / 2)

    printOnePair(verbose, metricNames, valueNames, runResults, alertThreshold, 'old', 'new', oldServer, newServer, versionMap, runs - 2 * throws)
    printOnePair(verbose, metricNames, valueNames, runResults, alertThreshold, 'T1E', 'T1O', oldServer, oldServer, versionMap, evenRuns)
    printOnePair(verbose, metricNames, valueNames, runResults, alertThreshold, 'T2E', 'T2O', newServer, newServer, versionMap, evenRuns)
    printOnePair(verbose, metricNames, valueNames, runResults, alertThreshold, 'T12E1', 'T12E2', oldServer, newServer, versionMap, evenRuns)
    printSummary(verbose, metricNames, valueNames, runResults, alertThreshold, oldServer, newServer, versionMap, evenRuns)
    return 0


if __name__ == '__main__':
    try:
        retVal = main()
    except KeyboardInterrupt:
        retVal = 2
    except Exception as exc:
        raise
        print(str(exc))
        retVal = 3

    sys.exit(retVal)
