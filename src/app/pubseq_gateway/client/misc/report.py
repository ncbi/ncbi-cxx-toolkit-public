#!/usr/bin/env python3

import argparse
import csv
from enum import IntEnum, auto
from functools import partial
from itertools import product
from pathlib import Path
import re
import shutil
import socket
from statistics import mean
import subprocess
import sys
from timeit import timeit

class EventType(IntEnum):
    START       = 0
    SUBMIT      = auto()
    REPLY       = auto()
    DONE        = auto()
    SEND        = 1000
    RECEIVE     = auto()
    CLOSE       = auto()
    RETRY       = auto()
    FAIL        = auto()

class EventIndex(IntEnum):
    FIRST       = 0
    LAST        = -1

class EventField(IntEnum):
    REQUEST_ID  = 0
    EPOCH       = auto()
    TYPE        = auto()
    THREAD_ID   = auto()
    REST        = auto()

rules = {
    'StartToDone':      [[ EventType.START,   EventIndex.FIRST ], [ EventType.DONE,    EventIndex.FIRST ]],
    'SendToClose':      [[ EventType.SEND,    EventIndex.FIRST ], [ EventType.CLOSE,   EventIndex.FIRST ]],

    'StartToSend':      [[ EventType.START,   EventIndex.FIRST ], [ EventType.SEND,    EventIndex.FIRST ]],
    'SendToFirstChunk': [[ EventType.SEND,    EventIndex.FIRST ], [ EventType.RECEIVE, EventIndex.FIRST ]],
    'FirstToLastChunk': [[ EventType.RECEIVE, EventIndex.FIRST ], [ EventType.RECEIVE, EventIndex.LAST  ]],
    'LastChunkToClose': [[ EventType.RECEIVE, EventIndex.LAST  ], [ EventType.CLOSE,   EventIndex.FIRST ]],
    'CloseToDone':      [[ EventType.CLOSE,   EventIndex.FIRST ], [ EventType.DONE,    EventIndex.FIRST ]],

    'StartToReply':     [[ EventType.START,   EventIndex.FIRST ], [ EventType.REPLY,   EventIndex.FIRST ]],
    'ReplyToDone':      [[ EventType.REPLY,   EventIndex.FIRST ], [ EventType.DONE,    EventIndex.FIRST ]],
}

class PerRequest:
    def __init__(self, enabled, file=sys.stdout):
        self._enabled = enabled

        if self._enabled:
            self._file = file
            self._row = []
            print('Request', *rules.keys(), 'Details', sep='\t', file=self._file)

    def add_to_row(self, time):
        if self._enabled:
            self._row.append(time)

    def print_row(self, request_id, details):
        if self._enabled:
            converted_row_values = [ f'{v:7.3f}' for v in self._row ]
            print(request_id, *converted_row_values, sep='\t', end='\t', file=self._file)

            def format_pair(key, value): return f'{key}' if value == 'Success' else f'{key}={value}'
            def use_pair(key, value): return key != 'Reply' or value != 'Success'
            formatted_details = [ format_pair(key, value) for key, values in details.items() for value in values if use_pair(key, value) ]
            print(*formatted_details, sep=',', file=self._file)
            self._row = []

class Statistics(list):
    def get_percentile(data, percentile):
        return data[int(percentile * len(data)) - 1]

    class Group(dict):
        def __init__(self, name=None, regex=None):
            self._name = name or regex
            self._regex = re.compile(regex) if regex else None
            self._data = {}

        def add(self, details, rule_name, time):
            for key, value in details.items():
                if not self._regex or self._regex.search(f'{key}={value}'):
                    self._data.setdefault(rule_name, []).append(time)
                    return True

            return False

        def calc(self):
            for rule_name, rule_data in self._data.items():
                rule_data.sort()
                self.setdefault('Min ', []).append(rule_data[0])
                self.setdefault('25th', []).append(Statistics.get_percentile(rule_data, 0.25))
                self.setdefault('Median', []).append(Statistics.get_percentile(rule_data, 0.50))
                self.setdefault('75th', []).append(Statistics.get_percentile(rule_data, 0.75))
                self.setdefault('90th', []).append(Statistics.get_percentile(rule_data, 0.90))
                self.setdefault('95th', []).append(Statistics.get_percentile(rule_data, 0.95))
                self.setdefault('Max ', []).append(rule_data[-1])
                self.setdefault('Average', []).append(mean(rule_data))

        def print(self, file):
            if self._name:
                print(f'{self._name}:', file=file)

            print('Statistics', *rules.keys(), sep='\t', file=file)

            for row_name, row_data in self.items():
                values = [ f'{v:7.3f}' for v in row_data ]
                print(row_name, *values, sep='\t', file=file)

            print(file=file)

    def __init__(self, groups, file=sys.stdout):
        if not isinstance(groups, list):
            super().__init__([])
        elif not groups:
            super().__init__([ self.Group() ])
        else:
            super().__init__([ self.Group(regex=regex) for regex in groups ] + [ self.Group(name='Everything else') ])

        self._file = file

    def add(self, details, rule_name, time):
        for group in self:
            if group.add(details, rule_name, time):
                break

    def print(self):
        for group in self:
            group.calc()
            group.print(self._file)

class Summary:
    def __init__(self, enabled, total, file=sys.stdout):
        self._enabled = enabled

        if self._enabled:
            self._min = None
            self._max = None
            self._success = 0
            self._total = total
            self._file = file

    def add(self, request_data, details):
        if self._enabled:
            start = request_data[EventType.START][EventIndex.FIRST][EventField.EPOCH]
            self._min = min(self._min or start, start)

            done = request_data[EventType.DONE][EventIndex.FIRST][EventField.EPOCH]
            self._max = max(self._max or done, done)

            try:
                if details['Reply'] != [ 'Success' ]: return
            except KeyError:
                # For psg_client v2.1.0 or older
                if details['success'] != [ 'true' ]: return

            self._success += 1

    def print(self):
        if self._enabled:
            print('Success', f'{self._success}/{self._total}', 'requests', sep='\t', file=self._file)
            print('Overall', f'{(self._max - self._min) / 1000:7.3f}', 'seconds', sep='\t', file=self._file)

def read_raw_metrics(input_file):
    reader = csv.reader(input_file, delimiter='\t')
    requests = {}

    for line in reader:
        if len(line) < EventField.REST:
            pass

        epoch = float(line[EventField.EPOCH])
        data = { EventField.EPOCH: epoch }

        if len(line) > EventField.REST:
            for key, value in [ s.split('=') for s in line[EventField.REST:] ]:
                data.setdefault(EventField.REST, {}).setdefault(key, []).append(value)

        request_id = line[EventField.REQUEST_ID]
        event_type = EventType(int(line[EventField.TYPE]))
        requests.setdefault(request_id, {}).setdefault(event_type, []).append(data)

    return requests

def report(requests, output_file, per_request, statistics, summary):
    per_request = PerRequest(per_request, file=output_file)
    statistics = Statistics(statistics, file=output_file)
    summary = Summary(summary, len(requests), file=output_file)

    for request_id, request_data in requests.items():
        details = request_data[EventType.DONE][EventIndex.FIRST][EventField.REST]

        for rule_name, rule_params in rules.items():
            ((start_type, start_index), (end_type, end_index)) = rule_params
            start = request_data[start_type][start_index.value][EventField.EPOCH]
            end = request_data[end_type][end_index.value][EventField.EPOCH]
            time = end - start
            per_request.add_to_row(time)
            statistics.add(details, rule_name, time)

        summary.add(request_data, details)
        per_request.print_row(request_id, details)

    print(file=output_file)
    statistics.print()
    summary.print()
    return statistics

def report_cmd(args):
    report_all = not args.per_request and args.statistics is None and not args.summary
    statistics_groups = args.statistics if isinstance(args.statistics, list) else [] if report_all else None
    requests = read_raw_metrics(args.input_file)
    report(requests, args.output_file, args.per_request or report_all, statistics_groups, args.summary or report_all)

def check_binaries(binaries):
    for bin in binaries:
        if shutil.which(bin) is None:
            sys.exit(f'"{bin}" does not exist or is not executable')

def create_dir(command, input_file, requests, runs):
    i = 1
    prefix = f'{Path(sys.argv[0]).stem}.{command}.{Path(input_file.name).name}_{requests}_{runs}'

    while True:
        new_dir = Path(f'{prefix}.{i:03d}')
        try:
            new_dir.mkdir()
            return new_dir
        except FileExistsError:
            i += 1

def create_input(path, input_file, requests):
    filename = Path(path) / f'input{Path(input_file.name).suffix}'

    with open(filename, 'w') as f:
        for i, line in enumerate(input_file):
            if i >= requests:
                break;
            f.write(line)

    return filename

def get_filename(path, suffix, service, user_threads, io_threads, requests_per_io, binary):
    safe_binary = binary.replace('/', '_')
    return Path(path) / f'{service}_{user_threads}_{io_threads}_{requests_per_io}_{safe_binary}.{suffix}.txt'

def performance_cmd(args, path, input_file, iter_args):
    statistics_to_output = {
            'median':   'Median time spent on sending a request and receiving corresponding reply, milliseconds',
            'average':  'Average time, milliseconds'
        }

    delay = hasattr(args, 'delay') and args.delay

    if delay:
        offending_values = list({ *args.user_threads } - { 1 })
        if offending_values:
            sys.exit(f'Delay ({args.delay}) only makes sense with a single user thread (offending -user-threads values: {offending_values})')

    def performance(run_no, service, user_threads, io_threads, requests_per_io, binary):
        output_file = get_filename(path, f'raw.{run_no}', service, user_threads, io_threads, requests_per_io, binary)
        cmd = [ binary, 'performance', '-output-file', output_file, '-service', service, '-user-threads', str(user_threads), '-io-threads', str(io_threads), '-requests-per-io', str(requests_per_io), '-verbose' ]

        if delay:
            cmd.extend([ '-delay', str(delay) ])

        with open(input_file, 'r') as f:
            subprocess.run(cmd, stdin=f, text=True)

    if args.warm_up:
        run_args = next(product(*iter_args.values()))
        performance('warm_up', *run_args)

    for run_no in range(args.RUNS):
        for run_args in product(*iter_args.values()):
            performance(run_no, *run_args)

    aggregate = {}

    for run_args in product(*iter_args.values()):
        requests = {}

        for run_no in range(args.RUNS):
            with open(get_filename(path, f'raw.{run_no}', *run_args)) as input_file:
                run_requests = read_raw_metrics(input_file)

                for key, value in run_requests.items():
                    requests[f'{key}.{run_no}'] = value

        with open(get_filename(path, 'pro', *run_args), 'w') as output_file:
            statistics = report(requests, output_file, True, [], True)[0]
            aggregate[run_args] = [ value[0] for key, value in statistics.items() if key.casefold() in statistics_to_output ]

    return aggregate, statistics_to_output

def overall_cmd(input_file_option, args, path, input_file, iter_args):
    statistics_to_output = {
            'median':   'Median elapsed real time, seconds',
            'average':  'Average time, seconds'
        }

    def overall(run_no, service, user_threads, io_threads, requests_per_io, binary):
        cmd = [ binary, args.command, input_file_option, input_file, '-service', service, '-worker-threads', str(user_threads), '-io-threads', str(io_threads), '-requests-per-io', str(requests_per_io) ]

        if args.save_output:
            with open(get_filename(path, f'raw.{run_no}', *run_args), 'w') as f:
                subprocess.run(cmd, stdout=f)
        else:
            subprocess.run(cmd, stdout=subprocess.DEVNULL)

        print('.', end='', flush=True)

    if args.warm_up:
        run_args = next(product(*iter_args.values()))
        overall('warm_up', *run_args)

    results = {}

    for run_no in range(args.RUNS):
        for run_args in product(*iter_args.values()):
            f = partial(overall, f'{run_no}', *run_args)
            result = timeit(f, number=1)
            results.setdefault(run_args, []).append(result)

    print(flush=True)

    aggregate = {}

    for run_args in product(*iter_args.values()):
        run_results = results[run_args]
        run_results.sort()

        with open(get_filename(path, 'pro', *run_args), 'w') as output_file:
            print(*[f'{v:.3f}' for v in run_results], sep='\n', file=output_file)

        aggregate[run_args] = [ Statistics.get_percentile(run_results, 0.50), mean(run_results) ]

    return aggregate, statistics_to_output

def resolve_cmd(*args):
    return overall_cmd('-id-file', *args)

def interactive_cmd(*args):
    return overall_cmd('-input-file', *args)

args_descriptions = {
        'service':          'PSG service/server name',
        'user_threads':     'Number of user threads working with PSG client API',
        'io_threads':       'Number of I/O threads working with PSG service/server',
        'requests_per_io':  'Number of requests consecutively given to the same I/O thread',
        'binary':           'Version of psg_client application'
    }

def create_result(path, iter_args, compared_iter_args, aggregate, statistics_to_output, additional_info):
    with open(path / 'result.csv', 'w', newline='') as output_file:
        writer = csv.writer(output_file)
        compared_headers = [ name for i, name in enumerate(iter_args.keys()) if compared_iter_args[i] ]
        writer.writerow(compared_headers + list(statistics_to_output.keys()))

        for run_args in product(*iter_args.values()):
            compared_args = [ value for i, value in enumerate(run_args) if compared_iter_args[i] ]
            writer.writerow(compared_args + [ f'{v:.3f}' for v in aggregate[run_args]])

        writer.writerows([ [], [ 'Legend:' ]])

        for i, desc in enumerate(args_descriptions.items()):
            if compared_iter_args[i]:
                writer.writerow(desc)

        for stat in statistics_to_output.items():
            writer.writerow(stat)

        writer.writerows([ [], [ 'Info:' ]])

        for i, desc in enumerate(args_descriptions.items()):
            if not compared_iter_args[i]:
                value = next(iter(iter_args[desc[0]]))
                writer.writerow([ desc[0], f'{desc[1]} ("{value}")' ])

        writer.writerows(additional_info)

def additional_info(args):
    yield from {
                'INPUT_FILE':   f'Input file ("{args.INPUT_FILE.name}")',
                'REQUESTS':     f'Number of requests per run ("{args.REQUESTS}")',
                'RUNS':         f'Number of runs ("{args.RUNS}")',
                'warm-up':      f'Whether to do a warm-up run before measurements ("{args.warm_up}")',
        }.items()

    if hasattr(args, 'delay'):
        yield   'delay',        f'Delay between requests, seconds ("{args.delay}")',

    yield       '',             f'Measurements performed on "{socket.gethostname()}"'

def run_cmd(args):
    check_binaries(args.binary)

    path = create_dir(args.command, args.INPUT_FILE, args.REQUESTS, args.RUNS)
    input_file = create_input(path, args.INPUT_FILE, args.REQUESTS)
    iter_args = { name: frozenset(vars(args)[name]) for name in args_descriptions.keys() }
    compared_iter_args = [ len(values) > 1 for i, values in enumerate(iter_args.values()) ]

    aggregate, statistics_to_output = globals()[f'{args.command}_cmd'](args, path, input_file, iter_args)

    create_result(path, iter_args, compared_iter_args, aggregate, statistics_to_output, additional_info(args))

parser = argparse.ArgumentParser()
subparsers = parser.add_subparsers(title='available commands', metavar='COMMAND', required=True, dest='command')

parser_report = subparsers.add_parser('report', help='Processes raw performance metrics and reports results', description='Processes raw performance metrics and reports results (everything by default)')
parser_report.set_defaults(func=report_cmd)
parser_report.add_argument('-input-file', help='Input file containing raw performance metrics (default: %(default)s)', metavar='FILE', default='-', type=argparse.FileType())
parser_report.add_argument('-output-file', help='Output file (default: %(default)s)', metavar='FILE', default='-', type=argparse.FileType('w'))
parser_report.add_argument('-per-request', help='Report per request results', action='store_true')
parser_report.add_argument('-statistics', help='Report statistics optionally grouping by request details (key=value) matching specified regex', metavar='REGEX', type=str, nargs='*')
parser_report.add_argument('-summary', help='Report summary', action='store_true')

for mode in [ 'resolve', 'interactive', 'performance' ]:
    parser_run = subparsers.add_parser(mode, help=f'Runs "psg_client {mode}" and aggregates results', description=f'Runs "psg_client {mode}" and aggregates results')
    parser_run.set_defaults(func=run_cmd)
    parser_run.add_argument('INPUT_FILE', help='File with requests', type=argparse.FileType())
    parser_run.add_argument('REQUESTS', help='Number of requests per run', type=int)
    parser_run.add_argument('RUNS', help='Number of runs', type=int)
    parser_run.add_argument('-service', help='PSG services to run against (default: %(default)s)', default=[ 'PSG2' ], nargs='+')
    parser_run.add_argument('-user-threads', help='Numbers of user threads to use (default: %(default)s)', metavar='THREADS', type=int, default=[ 6 ], nargs='+')
    parser_run.add_argument('-io-threads', help='Numbers of I/O threads to use (default: %(default)s)', metavar='THREADS', type=int, default=[ 6 ], nargs='+')
    parser_run.add_argument('-requests-per-io', help='Numbers of requests per I/O to use (default: %(default)s)', metavar='REQUESTS', type=int, default=[ 1 ], nargs='+')
    parser_run.add_argument('-binary', help='psg_client binaries to run', nargs='+', required=True)
    parser_run.add_argument('-warm-up', help='Whether to do a warm up run', action='store_true')

    if mode == 'performance':
        parser_run.add_argument('-delay', help='Delay between requests, seconds (default: %(default)s)', type=float, default=0.0)
    else:
        parser_run.add_argument('-save-output', help='Whether to save actual output', action='store_true')

args = parser.parse_args()
args.func(args)
