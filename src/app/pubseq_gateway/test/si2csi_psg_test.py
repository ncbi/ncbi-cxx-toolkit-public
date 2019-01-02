#!/usr/bin/env python3

import sys
import subprocess
import urllib.request
import json
import csv
from urllib.error import HTTPError
import time
from multiprocessing import Queue, Process

CASSANDRA_DUMP_FILE = './si2csi.cass.dump'
CONVERT_TO_FASTA = './convert_to_fasta.sh'
URL = 'http://nctest21:2180/ID/resolve?fmt=json&all_info=yes&'
CANCEL_JOIN_THREAD_TIMEOUT = 3
PARALLEL = 100

EXCLUSIONS = {}
EXCLUSIONS_SEQ_IDS = {}


count = 0
bar_space_count = 0
succeed = 0
parse_cass_error_count = 0
parse_fasta_error_count = 0
match_error_count = 0
match_error_indiv_count = 0


def getFasta(sec_seq_id, sec_seq_id_type):
    stdoutput = subprocess.getoutput(" ".join([CONVERT_TO_FASTA,
                                               "'--'",
                                               "'" + sec_seq_id + "'",
                                               "'" + sec_seq_id_type + "'"]))
    lines = stdoutput.splitlines()
    if len(lines) != 5:
        return None, None, None, None, None, stdoutput

    parts = lines[0].split(': ')
    if len(parts) != 2 or parts[0] != 'fasta':
        return None, None, None, None, None, stdoutput
    fasta = parts[1]

    parts = lines[1].split(': ')
    if len(parts) != 2 or parts[0] != 'fasta_content':
        return None, None, None, None, None, stdoutput
    fasta_content = parts[1]

    parts = lines[2].split(': ')
    if len(parts) != 2 or parts[0] != 'which':
        return None, None, None, None, None, stdoutput
    which = parts[1]

    parts = lines[3].split(': ')
    if len(parts) != 2 or parts[0] != 'fasta_type':
        return None, None, None, None, None, stdoutput
    fasta_type = parts[1]

    parts = lines[4].split(': ')
    if len(parts) != 2 or parts[0] != 'fasta_content_parsable':
        return None, None, None, None, None, stdoutput
    fasta_content_parsable = parts[1] == '1'

    return fasta, fasta_content, which, fasta_type, fasta_content_parsable, None


def testPSGResolution(fasta, fasta_content, which, fasta_type,
                      fasta_content_parsable,
                      accession, seq_id_type, version):
    """Sends a few requests to PSG and compares the output"""
    urls = []
    u1 = 'seq_id=' + urllib.parse.quote(fasta)
    if u1 not in EXCLUSIONS:
        urls.append(URL + u1)
    u2 = 'seq_id=' + urllib.parse.quote(fasta) + '&seq_id_type=' + which
    if u2 not in EXCLUSIONS:
        urls.append(URL + u2)
    if not fasta_content_parsable:
        if which != '11':
            u3 = 'seq_id=' + urllib.parse.quote(fasta_content)
            if u3 not in EXCLUSIONS:
                urls.append(URL + u3)
        u4 = 'seq_id=' + urllib.parse.quote(fasta_content) + '&seq_id_type=' + which
        if u4 not in EXCLUSIONS:
            urls.append(URL + u4)

    # fasta: gb|ATU25317|
    # fasta_content: ATU25317|
    alt_urls = []
    alt_fasta = fasta.replace('|', '||', 1)
    alt_fasta_content = '|' + fasta_content
    au1 = 'seq_id=' + urllib.parse.quote(alt_fasta)
    if au1 not in EXCLUSIONS:
        alt_urls.append(URL + au1)
    au2 = 'seq_id=' + urllib.parse.quote(alt_fasta) + '&seq_id_type=' + which
    if au2 not in EXCLUSIONS:
        alt_urls.append(URL + au2)

    au3 = 'seq_id=' + urllib.parse.quote(alt_fasta_content) + '&seq_id_type=' + which
    if au3 not in EXCLUSIONS:
        alt_urls.append(URL + au3)


    errors = []

    def alt_try_accession(url, accession):
        try:
            response = urllib.request.urlopen(url)
            content = response.read().decode('utf-8')
            alt_vals = json.loads(content)
            return alt_vals["accession"] == accession
        except:
            return False

    def alt_try_version(url, version):
        try:
            response = urllib.request.urlopen(url)
            content = response.read().decode('utf-8')
            alt_vals = json.loads(content)
            return str(alt_vals["version"]) == version
        except:
            return False

    def alt_try_seq_id_type(url, seq_id_type):
        try:
            response = urllib.request.urlopen(url)
            content = response.read().decode('utf-8')
            alt_vals = json.loads(content)
            return str(alt_vals["seq_id_type"]) == seq_id_type
        except:
            return False

    for url in urls:
        try:
            response = urllib.request.urlopen(url)
        except HTTPError as exc:
            errors.append(str(exc.code) + ": " + url)
            continue
        except Exception as exc:
            errors.append('???: ' + url + ': ' + str(exc))
            continue

        # {"accession": "XP_003591552", "version": 1, "seq_id_type": 10}
        content = response.read().decode('utf-8')
        vals = json.loads(content)
        if vals["accession"] != accession:
            alt_success = False
            for u in alt_urls:
                if alt_try_accession(u, accession):
                    alt_success = True
                    break
            if not alt_success:
                errors.append("Accession mismatch. Expected: " + accession + 
                              " received: " + vals["accession"] + " url: " + url)
                continue
        if str(vals["version"]) != version:
            alt_success = False
            for u in alt_urls:
                if alt_try_version(u, version):
                    alt_success = True
                    break
            if not alt_success:
                errors.append("Version mismatch. Expected: " + version + 
                              " received: " + str(vals["version"]) + " url: " + url)
                continue
        if str(vals["seq_id_type"]) != seq_id_type:
            alt_success = False
            for u in alt_urls:
                if alt_try_seq_id_type(u, seq_id_type):
                    alt_success = True
                    break
            if not alt_success:
                errors.append("Seq id type mismatch. Expected: " + seq_id_type + 
                              " received: " + str(vals["seq_id_type"]) + " url: " + url)
                continue

    return errors


def ProcessDBDumpLine(dumpLine, lineNumber, outputQueue):

    try:
        dumpLine = dumpLine.strip()

        vals = dumpLine.split(',')
        if len(vals) != 5:
            vals = list(csv.reader([dumpLine]))[0]
            if len(vals) != 5:
                err = "Error: expected exactly 5 fields. Line number: " + \
                      str(lineNumber) + " Line Content: " + dumpLine
                global parse_cass_error_count
                parse_cass_error_count += 1

                outputQueue.put({'error': err})
    #            time.sleep(CANCEL_JOIN_THREAD_TIMEOUT)
    #            outputQueue.cancel_join_thread()
                return

        sec_seq_id = vals[0]
        sec_seq_id_type = vals[1]

        accession = vals[2]
        seq_id_type = vals[3]
        version = vals[4]

        if sec_seq_id in EXCLUSIONS_SEQ_IDS:
            outputQueue.put({'error': None})
            return
            

        # Convert first two items to fasta
        fasta, fasta_content, which, fasta_type, fasta_content_parsable, stdoutput = \
            getFasta(sec_seq_id, sec_seq_id_type)
        if fasta is None:
            err = "Error: cannot convert to fasta. Line number: " + \
                  str(lineNumber) + " Line Content: " + dumpLine + \
                  " Converter stdout: " + stdoutput
            global parse_fasta_error_count
            parse_fasta_error_count += 1

            outputQueue.put({'error': err})
    #        time.sleep(CANCEL_JOIN_THREAD_TIMEOUT)
    #        outputQueue.cancel_join_thread()
            return

        # Get resolution reply from PSG
        errors = testPSGResolution(fasta, fasta_content, which, fasta_type,
                                   fasta_content_parsable,
                                   accession, seq_id_type, version)
        if errors:
            global match_error_count
            match_error_count += 1
            err = "Error matching PSG reply. Line number: " + \
                  str(lineNumber) + " Line Content: " + dumpLine + \
                  " Fasta: " + str(fasta) + \
                  " Fasta content: " + str(fasta_content) + \
                  " Fasta type: " + str(fasta_type) + \
                  " Which: " + str(which)
            for e in errors:
                global match_error_indiv_count
                match_error_indiv_count += 1

                err += '\n' + " -- " + e
            outputQueue.put({'error': err})
        else:
            global succeed
            succeed += 1
            outputQueue.put({'error': None})

    #    time.sleep(CANCEL_JOIN_THREAD_TIMEOUT)
    #    outputQueue.cancel_join_thread()
        return
    except KeyboardInterrupt as exc:
        outputQueue.put({'error': None})
    except Exception as exc:
        outputQueue.put({'error': str(exc)})


def ProcessDBLines(items):
    outputQueue = Queue()
    procs = []
    for item in items:
        proc = Process(target=ProcessDBDumpLine,
                       args=(item[0], item[1], outputQueue))
        procs.append(proc)
        proc.start()

    for i in range(len(procs)):
        output = outputQueue.get()
        err = output['error']
        if err is not None:
            print(err)
            print()

    for proc in procs:
        proc.join()


try:
    # Read exclusions
    for exclusion in sys.argv[1:]:
        with open(exclusion) as ex_file:
            for line in ex_file:
                line = line.strip()
                if line:
                    if not line.startswith('#'):
                        if line.upper().startswith('SEQ_ID:'):
                            EXCLUSIONS_SEQ_IDS[line[len('SEQ_ID:'):].strip()] = None
                        else:
                            EXCLUSIONS[line] = None

    items = []
    with open(CASSANDRA_DUMP_FILE) as f:
        for line in f:
            if not line:
                continue
            count += 1

            if '| ' in line:
                bar_space_count += 1
                line = line.replace('| ', '|')

            if len(items) >= PARALLEL:
                ProcessDBLines(items)
                items = []
            else:
                items.append([line, count])

    if items:
        ProcessDBLines(items)
        items = []



except KeyboardInterrupt as exc:
    print()
    print("Exiting, Ctrl+C received")
    print()
except Exception as exc:
    print()
    print("Exiting, execution exception: " + str(exc))
    print()
except:
    print()
    print("Exiting, unknown exception")
    print()



print("Total line processed: " + str(count))
print("DB lines with '| ' in it (replaced with '|'): " + str(bar_space_count))
print("Succeeded: " + str(succeed))
print("Parse cassandra line error count: " + str(parse_cass_error_count))
print("Parse convert to fasta error count: " + str(parse_fasta_error_count))
print("Match error count: " + str(match_error_count))
print("Individual match error count: " + str(match_error_indiv_count))

