#!/usr/bin/env python3

import sys, os
from optparse import OptionParser
from multiprocessing import Process


def runResolve(accession, fake):
    cmd = './nghttp -n "http://localhost:2180/ID/resolve?accession=' + \
          accession + '"'
    retCode  = os.system(cmd)
    if retCode != 0:
        print('\nERROR resolving ' + accession + ': ' + str(retCode))

def runGetBlob(satKey, fake):
    cmd = './nghttp -n "http://localhost:2180/ID/getblob?sat=4&sat_key=' + \
          str(satKey) + '"'
    retCode  = os.system(cmd)
    if retCode != 0:
        print('\nERROR retrieving ' + str(satKey) + ': ' + str(retCode))


def process(accessions):
    # Create processes
    procs = []
    for accession in accessions:
        proc = Process(target=runResolve,
                       args=(accession, True))
        procs.append(proc)

    # Run processes
    for proc in procs:
        proc.start()

    # Join processes
    for proc in procs:
        proc.join()


def processSatKeys(satKeys):
    # Create processes
    procs = []
    for satKey in satKeys:
        proc = Process(target=runGetBlob,
                       args=(satKey, True))
        procs.append(proc)

    # Run processes
    for proc in procs:
        proc.start()

    # Join processes
    for proc in procs:
        proc.join()


def main():

    parser = OptionParser(
    """
    %prog <number of parallel requests> <file with accessions> <file with sat_key>
    """)

    parser.add_option("-v", "--verbose",
                      action="store_true", dest="verbose", default=False,
                      help="be verbose (default: False)")
    parser.add_option("--limit-resolves",
                      dest="limitResolves", default="-1",
                      help="Limit the number of resolves (negative => unlimited")
    parser.add_option("--limit-retrieves",
                      dest="limitRetrieves", default="-1",
                      help="Limit the number of retrieves (negative => unlimited")

    options, args = parser.parse_args()
    if len(args) != 3:
        print('bad arguments')
        return 1

    processes = int(args[0])
    fAccName = args[1]
    fSatKeyName = args[2]
    limitResolves = int(options.limitResolves)
    limitRetrieves = int(options.limitRetrieves)

    if limitResolves != 0:
        count = 0
        with open(fAccName) as f:
            accessions = []
            for line in f:
                line = line.strip()
                if not line:
                    continue
                count += 1
                accessions.append(line)
                if len(accessions) >= processes:
                    process(accessions)
                    accessions = []
                if limitResolves >= 0:
                    if count >= limitResolves:
                        break

            if accessions:
                process(accessions)
                accessions = []

        print('Processed ' + str(count) + ' accessions')
    else:
        print('Skipping resolve')

    if limitRetrieves != 0:
        count = 0
        with open(fSatKeyName) as f:
            satKeys = []
            for line in f:
                line = line.strip()
                if not line:
                    continue
                count += 1
                satKeys.append(line)
                if len(satKeys) >= processes:
                    processSatKeys(satKeys)
                    satKeys = []
                if limitRetrieves >= 0:
                    if count >= limitRetrieves:
                        break

            if satKeys:
                processSatKeys(satKeys)
                satKeys = []

        print('Processed ' + str(count) + ' sat keys')
    else:
        print('Skipping getblob')

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
