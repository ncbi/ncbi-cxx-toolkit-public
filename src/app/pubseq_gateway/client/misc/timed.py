# ===========================================================================
#
#                            PUBLIC DOMAIN NOTICE
#               National Center for Biotechnology Information
#
#  This software/database is a "United States Government Work" under the
#  terms of the United States Copyright Act.  It was written as part of
#  the author's official duties as a United States Government employee and
#  thus cannot be copyrighted.  This software/database is freely available
#  to the public for use. The National Library of Medicine and the U.S.
#  Government have not placed any restriction on its use or reproduction.
#
#  Although all reasonable efforts have been taken to ensure the accuracy
#  and reliability of the software and data, the NLM and the U.S.
#  Government do not and cannot warrant the performance or results that
#  may be obtained by using this software or data. The NLM and the U.S.
#  Government disclaim all warranties, express or implied, including
#  warranties of performance, merchantability or fitness for any particular
#  purpose.
#
#  Please cite the author in any work or product based on this material.
#
# ===========================================================================
#
# Author: Rafael Sadyrov
#
#!/usr/bin/env python3

import argparse
import asyncio
import collections
import csv
from datetime import datetime
import itertools
import os
import random
import re
import shlex
import shutil
from statistics import mean, median, stdev
import subprocess
import sys
import tempfile

class RegexCounter:
    def __init__(self, regexes):
        self._total = {r: [0] for r in regexes}

    def _process(self, line):
        for r, total in self._total.items():
            if r.regex is None or r.regex.search(line):
                total[0] += 1

    async def process_async(self, reader):
        while True:
            if line := await reader.readline():
                self._process(line)
            else:
                break

    def process_sync(self, file):
        file.seek(0)

        for line in file:
            self._process(line)

    @property
    def total(self):
        return self._total

Combination = collections.namedtuple('Combination', 'env, cmd, id')

def get_combinations(time_measurements, regexes, input_file, args):
    def get_safe(name):
        return name.lstrip('-./').translate(str.maketrans('.', '_'))

    arg = 0
    def get_args(args):
        try:
            name, *values = shlex.split(args.pop())
            combinations = get_args(args)

            if values:
                if name.startswith('-'):
                    return [Combination(c.env, c.cmd + [name, v], c.id + ((name, v),)) for c in combinations for v in values]
                elif name.startswith('='):
                    return [Combination(c.env | {name[1:]: v}, c.cmd, c.id + ((name, v),)) for c in combinations for v in values]
                else:
                    nonlocal arg
                    arg += 1
                    return [Combination(c.env, c.cmd + [v], c.id + ((f'arg{arg}', v),)) for c in combinations for v in [name] + values]
            else:
                return [Combination(c.env, c.cmd + [name], c.id) for c in combinations]
        except IndexError:
            return [Combination({}, [], ())]

    field_names = []
    rv = []
    for line in input_file:
        cmds, *cmd_args = shlex.split(line)

        for cmd in shlex.split(cmds):
            if shutil.which(cmd) is None:
                sys.exit(f'"{cmd}" does not exist or is not executable')

            combinations = get_args(cmd_args)
            for c in combinations:
                field_names += [v[0] for v in c.id]
                rv.append(Combination(c.env, [cmd] + args + c.cmd, (("Executable", get_safe(cmd)),) + c.id))

    # Find common prefix for env. variables
    env_prefix = os.path.commonprefix(tuple(v for v in field_names if v[0] == '='))

    # Add human-readable variants
    field_names = {v: v.lstrip('-').removeprefix(env_prefix).title() for v in field_names}

    # Replace names in id with human-readable variants
    rv = [Combination(c.env, c.cmd, tuple((field_names.get(n, n), v) for n, v in c.id)) for c in rv]

    field_names = {'Executable': None} | {v: k for k, v in field_names.items()} | {'Which': 'Run / statistic'}
    field_names |= {r.name: r.regex.pattern.decode() if r.regex else '' for values in regexes.values() for r in values}
    field_names |= {'Failures': 'Failures reported' for m in time_measurements}
    field_names |= {m.name: m.desc for m in time_measurements}

    random.shuffle(rv)
    return (field_names, rv)

Regex = collections.namedtuple('Regex', 'name, regex')

def get_regexes(args):
    get_name = lambda n, r: '{}.{}'.format(n, f'Regex.{r.pattern.decode()}' if r else 'Lines')
    prepare = lambda n, vs: tuple(Regex(get_name(n, r), r) for r in itertools.chain((None,), map(re.compile, map(str.encode, vs))))
    return { n: prepare(n, vs) for n, vs in (('Out', args.stdout_re), ('Err', args.stderr_re))}

def run_combination(cmd, regexes, env, args):
    stdout_rc = RegexCounter(regexes['Out'])
    stderr_rc = RegexCounter(regexes['Err'])

    async def _async():
        process = await asyncio.create_subprocess_exec(*cmd, stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.PIPE, env=env)

        stdout_task = asyncio.create_task(stdout_rc.process_async(process.stdout))
        stderr_task = asyncio.create_task(stderr_rc.process_async(process.stderr))

        await asyncio.gather(stdout_task, stderr_task)
        await process.wait()
        assert process.returncode == 0, f"Command '{cmd}' returned non-zero exit status {process.returncode}."

    def _sync():
        with tempfile.NamedTemporaryFile(dir=args.tmp_dir) as stdout, tempfile.NamedTemporaryFile(dir=args.tmp_dir) as stderr:
            subprocess.run(cmd, stdout=stdout, stderr=stderr, env=env).check_returncode()

            stdout_rc.process_sync(stdout)
            stderr_rc.process_sync(stderr)

    asyncio.run(_async()) if args.asyncio else _sync()
    return (stdout_rc.total, stderr_rc.total)

def print_run_details(c, run):
    print(datetime.now().isoformat(timespec='seconds'), ' '.join(('='.join(v) for v in c.id)), '='.join(('run', str(run))))

Measure = collections.namedtuple('Measure', 'name, desc, fmt')

def measure(args):
    time_measurements = (
            Measure('Command', 'Command', '%C'),
            Measure('Status', 'Exit status', '%x'),
            Measure('Elapsed', 'Elapsed real time (seconds)', '%e'),
            Measure('System', 'System time (seconds)', '%S'),
            Measure('User', 'User time (seconds)', '%U'),
            Measure('CPU', 'CPU (%%)', '%P'),
            Measure('Max. Res', 'Maximum resident set size (KB)', '%M'),
            Measure('Avg. Res', 'Average resident set size (KB)', '%t'),
            Measure('Avg. Total', 'Average total size (KB)', '%K'),
            Measure('Avg. Data', 'Average data size (KB)', '%D'),
            Measure('Avg. Stack', 'Average stack size (KB)', '%p'),
            Measure('Avg. Text', 'Average text size (KB)', '%X'),
            Measure('Major Faults', 'Major page faults', '%F'),
            Measure('Minor Faults', 'Minor page faults', '%R'),
            Measure('Swap', 'Swaps', '%W'),
            Measure('Vol. Switches', 'Voluntary context switches', '%w'),
            Measure('Inv. Switches', 'Involuntary context switches', '%c'),
            Measure('Inputs', 'File system inputs', '%I'),
            Measure('Outputs', 'File system outputs', '%O'),
            Measure('Received', 'Socket messages received', '%r'),
            Measure('Sent', 'Socket messages sent', '%s'),
            Measure('Signals', 'Signals delivered', '%k')
        )

    time_format = '\n'.join(f'{m.name},{m.fmt}' for m in time_measurements)
    time = [shutil.which('time'), '-f', time_format, '-o']
    regexes = get_regexes(args)
    field_names, combinations = get_combinations(time_measurements, regexes, args.input_file, args.ARGS)

    if args.warm_up:
        c = combinations[-1]
        print_run_details(c, 'warm-up')
        subprocess.run(c.cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, env=c.env)

    writer = csv.DictWriter(args.output_file, fieldnames=field_names.keys(), quoting=csv.QUOTE_NONNUMERIC)
    writer.writeheader()

    for run in range(args.RUNS):
        for c in combinations:
            print_run_details(c, run)

            with tempfile.NamedTemporaryFile(dir=args.tmp_dir, mode='r', newline='') as time_file:
                out_total, err_total = run_combination(time + [time_file.name] + c.cmd, regexes, c.env, args)

                data = dict(c.id)
                data |= {'Which': run }
                data |= {r.name: v[0] for r, v in (out_total | err_total).items()}

                for row in csv.reader(time_file):
                    try:
                        name, value = row

                        if name == 'CPU':
                            value = float(value.rstrip('%'))
                        elif name == 'Command':
                            value = ' '.join('='.join((k, v)) for k, v in c.env.items()) + ' ' + value
                        else:
                            value = float(value)

                    except:
                        # Add run failures
                        name, value = ('Failures', row[0])

                    data |= {name: value}

                writer.writerow(data)

            args.output_file.flush()

    writer.writerow({})
    writer.writerow(field_names)

def process(args):
    reader = csv.reader(args.input_file, quoting=csv.QUOTE_NONNUMERIC)
    field_names = {name: i for i, name in enumerate(next(reader))}
    data = [row for row in reader]
    field_desc = {f: d for f, d in zip(field_names, data.pop())}
    data.pop()
    data.sort()

    which_index = field_names['Which']

    # Group values by run combination
    data = {c: list(g) for c, g in itertools.groupby(data, key=lambda x: tuple(x[:which_index]))}

    # Group values by measurement and run combination
    data = {m: {c: [v[field_names[m]] for v in vs] for c, vs in data.items()} for m in tuple(field_names)[which_index:]}

    # Remove measurements with all empty/zero values
    if not args.empty:
        data = {m: vs for m, vs in data.items() if any(itertools.chain.from_iterable(vs.values()))}
        field_names = {n: i for i, (n, _) in enumerate(filter(lambda p: p[1] < which_index or p[0] in tuple(data), field_names.items()))}
        field_desc = dict(filter(lambda p: p[0] in field_names, field_desc.items()))

    values_len = len(tuple(tuple(data.values())[0].values())[0])

    # Add statistics if possible
    if values_len > 1:
        def get_stats(measurement, values):
            if measurement == 'Which':
                return ['Min', 'Median', 'Mean', 'StdDev']
            elif measurement in ('Command', 'Exit status', 'Failures'):
                return [values[0]] * 4
            else:
                m = mean(values)
                return [min(values), median(values), m, stdev(values, xbar=m)]

        values_len += 4
        data = {m: {c: v + get_stats(m, v) for c, v in vs.items()} for m, vs in data.items()}

    # Group values by run combination
    data = sorted(tuple((c, (m, v)) for m, vs in data.items() for c, v in vs.items()))
    data = {c: {k: v for _, (k, v) in g} for c, g in itertools.groupby(data, key=lambda x: x[0])}

    writer = csv.writer(args.output_file, quoting=csv.QUOTE_NONNUMERIC)
    writer.writerow(field_names)

    for c, vs in data.items():
        for i in range(values_len):
            writer.writerow(c + tuple(vs[m][i] for m in tuple(field_names)[which_index:]))

    writer.writerow([])
    writer.writerow(field_desc.values())

parser = argparse.ArgumentParser()
subparsers = parser.add_subparsers(title='available commands', metavar='COMMAND', required=True, dest='command')

measure_parser = subparsers.add_parser('measure', help='Measure resource usage using "time"', description='Measure resource usage by command combinations using "time".')
measure_parser.set_defaults(func=measure)
measure_parser.add_argument('-warm-up', help='Whether to do a warm up run', action='store_true')
measure_parser.add_argument('-stdout-re', help='Regex to count on stdout', action='append', default=[])
measure_parser.add_argument('-stderr-re', help='Regex to count on stderr', action='append', default=[])
measure_parser.add_argument('-input-file', '-i', help='Input text file with command combinations to measure (default: %(default)s)', metavar='FILE', default='-', type=argparse.FileType())
measure_parser.add_argument('-output-file', '-o', help='Output CSV file (default: %(default)s)', metavar='FILE', default='-', type=argparse.FileType('x'))
measure_group = measure_parser.add_mutually_exclusive_group()
measure_group.add_argument('-tmp-dir', help='Temporary directory to use (default: %(default)s)', default='/dev/shm')
measure_group.add_argument('-asyncio', help='Whether to use asyncio', action='store_true')
measure_parser.add_argument('RUNS', help='Number of runs per command combination', type=int)
measure_parser.add_argument('ARGS', help='Options common to all command combinations', nargs='*')

process_parser = subparsers.add_parser('process', help='Process prior measurements', description='Process prior measurements (add some statistics, remove empty columns, etc).')
process_parser.set_defaults(func=process)
process_parser.add_argument('-input-file', '-i', help='Input CSV file (default: %(default)s)', metavar='FILE', default='-', type=argparse.FileType())
process_parser.add_argument('-output-file', '-o', help='Output CSV file (default: %(default)s)', metavar='FILE', default='-', type=argparse.FileType('w'))
process_parser.add_argument('-empty', help='Keep zero/empty measurements', action='store_true')

args = parser.parse_args()
args.func(args)
