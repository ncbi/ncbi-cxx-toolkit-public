#!/usr/bin/env python3

import argparse
import contextlib
import csv
from enum import IntEnum, auto
import io
from itertools import product
import os
from pathlib import Path
import re
import shutil
import socket
from statistics import mean, quantiles, stdev, StatisticsError
import subprocess
import sys

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
    _percentiles_steps = 100 // 5 # 5 is the greatest common divisor of the percentiles
    _percentiles = {
            '25th':   0.25,
            'Median': 0.50,
            '75th':   0.75,
            '90th':   0.90,
            '95th':   0.95,
        }

    class Group(dict):
        def __init__(self, name=None, regex=None):
            self._name = name or regex
            self._regex = re.compile(regex) if regex else None
            self._data = {}
            self._file = io.StringIO()

            if self._name:
                print(f'{self._name}:', file=self._file)

            print('Statistics', *rules.keys(), sep='\t', file=self._file)

        def add(self, details, rule_name, time):
            if not self._regex or any(self._regex.search(f'{key}={value}') for key, values in details.items() for value in values):
                self._data.setdefault(rule_name, []).append(time)
                return True

            return False

        def print(self):
            for rule_name, rule_data in self._data.items():
                self.setdefault('Min ', []).append(min(rule_data))

                try:
                    q = quantiles(rule_data, n=Statistics._percentiles_steps, method='inclusive')

                    for name, value in Statistics._percentiles.items():
                        self.setdefault(name, []).append(q[int(Statistics._percentiles_steps * value) - 1])
                except StatisticsError:
                    # rule_data has fewer than two values
                    for name, value in Statistics._percentiles.items():
                        self.setdefault(name, []).append(rule_data[0])

                self.setdefault('Max ', []).append(max(rule_data))
                m = mean(rule_data)
                self.setdefault('Average', []).append(m)
                self.setdefault('StdDev', []).append(stdev(rule_data, xbar=m))

            for row_name, row_data in self.items():
                values = [ f'{v:7.3f}' for v in row_data ]
                print(row_name, *values, sep='\t', file=self._file)

        def clear(self):
            self._data.clear()
            super().clear()

        def summary(self, file):
            self._file.seek(0)
            for line in self._file:
                print(line, end='', file=file)

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
            group.print()

    def clear(self):
        for group in self:
            group.clear()

    def summary(self):
        for group in self:
            group.summary(self._file)
            print(file=self._file)

class Summary:
    def __init__(self, enabled, file=sys.stdout):
        self._enabled = enabled

        if self._enabled:
            self._min = None
            self._max = None
            self._success = 0
            self._total = 0
            self._file = file

    def add(self, request_data, details):
        if self._enabled:
            start = request_data[EventType.START][EventIndex.FIRST][EventField.EPOCH]
            self._min = min(self._min or start, start)

            done = request_data[EventType.DONE][EventIndex.FIRST][EventField.EPOCH]
            self._max = max(self._max or done, done)
            self._total += 1

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
        try:
            if len(line) < EventField.REST:
                pass

            epoch = float(line[EventField.EPOCH])
            data = { EventField.EPOCH: epoch }

            if len(line) > EventField.REST:
                for key, value in [ s.split('=') for s in line[EventField.REST:] ]:
                    data.setdefault(EventField.REST, {}).setdefault(key, []).append(value)

            request_id = line[EventField.REQUEST_ID]
            event_type = EventType(int(line[EventField.TYPE]))

        except:
            continue

        requests.setdefault(request_id, {}).setdefault(event_type, []).append(data)
        event_types = requests[request_id].keys()

        if (event_type == EventType.DONE and (EventType.CLOSE in event_types or EventType.FAIL in event_types) or
                event_type in {EventType.CLOSE, EventType.FAIL} and EventType.DONE in event_types):
            yield request_id, requests.pop(request_id)

def report(requests, output_file, per_request, statistics, progress, summary):
    per_request = PerRequest(per_request, file=output_file)
    statistics = Statistics(statistics, file=output_file)
    summary = Summary(summary, file=output_file)

    for i, (request_id, request_data) in enumerate(requests, start=1):
        data = []

        try:
            details = request_data[EventType.DONE][EventIndex.FIRST][EventField.REST]

            for rule_name, rule_params in rules.items():
                ((start_type, start_index), (end_type, end_index)) = rule_params
                start = request_data[start_type][start_index.value][EventField.EPOCH]
                end = request_data[end_type][end_index.value][EventField.EPOCH]
                time = end - start
                data.append([details, rule_name, time])
        except:
            pass
        else:
            for line in data:
                per_request.add_to_row(line[-1])
                statistics.add(*line)

            summary.add(request_data, details)
            per_request.print_row(request_id, details)

        if progress and i % progress == 0:
            statistics.print()
            statistics.clear()

    print(file=output_file)

    if not progress:
        statistics.print()

    statistics.summary()
    summary.print()
    return statistics

def report_cmd(args):
    report_all = not args.per_request and args.statistics is None and not args.progress and not args.summary
    statistics_groups = args.statistics if isinstance(args.statistics, list) else [] if report_all or args.progress else None
    requests = read_raw_metrics(args.input_file)
    report(requests, args.output_file, args.per_request or report_all, statistics_groups, args.progress, args.summary or report_all)

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

def get_filename(path, suffix, service, user_threads, io_threads, requests_per_io, binary, rate=0):
    safe_binary = binary.replace('/', '_')
    if io_threads is None:
        io_threads = '-'
    if requests_per_io is None:
        requests_per_io = '-'
    return Path(path) / f'{service}_{user_threads}_{io_threads}_{requests_per_io}_{safe_binary}_{rate}.{suffix}.txt'

def performance_cmd(args, path, input_file, iter_args):
    statistics_to_output = {
            'median':   'Median time spent on sending a request and receiving corresponding reply, milliseconds',
            'average':  'Average time, milliseconds',
            'stddev':   'Standard deviation, milliseconds'
        }

    delay = hasattr(args, 'delay') and args.delay

    if delay:
        offending_values = list({ *args.user_threads } - { 1 })
        if offending_values:
            sys.exit(f'Delay ({args.delay}) only makes sense with a single user thread (offending -user-threads values: {offending_values})')

    def performance(run_no, service, user_threads, io_threads, requests_per_io, binary):
        output_file = get_filename(path, f'raw.{run_no}', service, user_threads, io_threads, requests_per_io, binary)
        io_threads = [] if io_threads is None else ['-io-threads', str(io_threads)]
        requests_per_io = [] if requests_per_io is None else ['-requests-per-io', str(requests_per_io)]
        conf_file = binary + '.ini'
        conf = [ '-conffile', conf_file ] if os.path.isfile(conf_file) else []
        verbose = [] if args.save_stderr else [ '-verbose' ]
        cmd = [ binary, 'performance', '-output-file', output_file, '-service', service, '-user-threads', str(user_threads), *io_threads, *requests_per_io, *verbose, *conf ]

        if delay:
            cmd.extend([ '-delay', str(delay) ])

        open_stdin = lambda: open(input_file, 'r')
        open_stderr = lambda: open(get_filename(path, f'err.{run_no}', *run_args), 'w') if args.save_stderr else contextlib.nullcontext()

        with open_stdin() as stdin, open_stderr() as stderr:
            stderr = stderr if args.save_stderr else None
            subprocess.run(cmd, stdin=stdin, stderr=stderr, text=True)

        if args.save_stderr:
            print('.', end='', flush=True)

    def adjusted_requests():
        for run_no in range(args.RUNS):
            with open(get_filename(path, f'raw.{run_no}', *run_args)) as input_file:
                run_requests = read_raw_metrics(input_file)

                for key, value in run_requests:
                    yield f'{key}.{run_no}', value

    if args.warm_up:
        run_args = next(product(*iter_args.values()))
        performance('warm_up', *run_args)

    for run_no in range(args.RUNS):
        for run_args in product(*iter_args.values()):
            performance(run_no, *run_args)

    if args.save_stderr:
        print(flush=True)

    aggregate = {}

    for run_args in product(*iter_args.values()):
        requests = adjusted_requests()

        with open(get_filename(path, 'pro', *run_args), 'w') as output_file:
            statistics = report(requests, output_file, True, [], 0, True)[0]
            aggregate[run_args] = [ value[0] for key, value in statistics.items() if key.casefold() in statistics_to_output ]

    return aggregate, statistics_to_output

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
                if value is not None:
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
    if args.command != 'performance':
        args_descriptions['rate'] = 'Maximum number of requests to submit per second'

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
parser_report.add_argument('-progress', help='Report statistics every N requests (default: %(default)s)', metavar='N', default=0, type=int)
parser_report.add_argument('-summary', help='Report summary', action='store_true')

for mode in [ 'performance' ]:
    parser_run = subparsers.add_parser(mode, help=f'Runs "psg_client {mode}" and aggregates results', description=f'Runs "psg_client {mode}" and aggregates results')
    parser_run.set_defaults(func=run_cmd)
    parser_run.add_argument('INPUT_FILE', help='File with requests', type=argparse.FileType())
    parser_run.add_argument('REQUESTS', help='Number of requests per run', type=int)
    parser_run.add_argument('RUNS', help='Number of runs', type=int)
    parser_run.add_argument('-service', help='PSG services to run against (default: %(default)s)', default=[ 'PSG2' ], nargs='+')
    parser_run.add_argument('-user-threads', help='Numbers of user threads to use (default: %(default)s)', metavar='THREADS', type=int, default=[ 6 ], nargs='+')
    parser_run.add_argument('-io-threads', help='Numbers of I/O threads to use (default: %(default)s)', metavar='THREADS', type=int, default=[ None ], nargs='+')
    parser_run.add_argument('-requests-per-io', help='Numbers of requests per I/O to use (default: %(default)s)', metavar='REQUESTS', type=int, default=[ None ], nargs='+')
    parser_run.add_argument('-binary', help='psg_client binaries to run', nargs='+', required=True)
    parser_run.add_argument('-warm-up', help='Whether to do a warm up run', action='store_true')
    parser_run.add_argument('-save-stderr', help='Whether to save stderr', action='store_true')

    if mode == 'performance':
        parser_run.add_argument('-delay', help='Delay between requests, seconds (default: %(default)s)', type=float, default=0.0)
    else:
        parser_run.add_argument('-save-output', help='Whether to save actual output', action='store_true')
        parser_run.add_argument('-rate', help='Maximum number of requests to submit per second (default: %(default)s)', type=int, default=[ 0 ], nargs='+')

args = parser.parse_args()
args.func(args)
