#!/usr/bin/env python3

import argparse
import ast
import csv
import collections
from contextlib import contextmanager
from difflib import unified_diff
import enum
import functools
import itertools
import json
import operator
import os
import queue
import random
import re
import shutil
import subprocess
import sys
import tempfile
import threading
import time
import xml.etree.ElementTree as ET

print = functools.partial(print, flush=True)

class RequestGenerator:
    def __init__(self):
        self._request_no = 0

    def __call__(self, method, **params):
        self._request_no += 1
        request_id = f'{method}_{self._request_no}'
        return { 'jsonrpc': '2.0', 'method': method, 'params': params, 'id': request_id }, request_id 

class Deadline:
    def __init__(self, timeout=None):
        self._value = None if timeout is None else timeout + time.monotonic()

    def __or__(self, other):
        if self._value is None:
            return other
        elif other._value is None:
            return self
        else:
            return self if self._value <= other._value else other

    def get_remaining_or_raise(self):
        if self._value is None:
            return None

        timeout = self._value - time.monotonic()

        if timeout > 0.0:
            return timeout

        raise queue.Empty

@functools.total_ordering
class Version:
    def __init__(self, value=None):
        self._data = tuple(value.split('.')) if value else ()

    def _next_ne(self, other):
        return next(filter(lambda args : operator.ne(*args), itertools.zip_longest(self._data, other._data, fillvalue='0')), None)
        return rv

    def __eq__(self, other):
        return self._next_ne(other) is None

    def __lt__(self, other):
        if n := self._next_ne(other):
            a, b = n
            return int(a) < int(b) if a.isnumeric() and b.isnumeric() else a < b
        else:
            return False

    def __bool__(self):
        return bool(self._data)

class PsgClient:
    VerboseLevel = enum.IntEnum('VerboseLevel', 'REQUEST RESPONSE DEBUG')

    def __init__(self, args):
        self._cmd = [args.binary, 'interactive', '-server-mode']
        self._env = {}
        self._verbose = args.verbose
        self._sent = {}
        self._queue = queue.Queue()

        args_dict = vars(args)

        self._detect_server_version = (server_version := args_dict.get('server_version')) is None
        self._server_version = Version(server_version)

        if service := args_dict.get('service'):
            self._cmd += ['-service', service]

        if args_dict.get('https'):
            self._cmd += ['-https']

        if data_limit := args_dict.get('data_limit'):
            self._cmd += ['-data-limit', data_limit]

        if preview_size := args_dict.get('preview_size'):
            self._cmd += ['-preview-size', preview_size]

        if one_server := args_dict.get('one_server'):
            self._cmd += ['-one-server']

        if testing := args_dict.get('testing'):
            self._cmd += ['-testing']

        if self._verbose >= PsgClient.VerboseLevel.DEBUG:
            self._cmd += ['-debug-printout', 'some']
            print(self._cmd)

    def __enter__(self):
        if self._env:
            env = os.environ.copy()
            env.update(self._env)
        else:
            env = None

        self._pipe = subprocess.Popen(self._cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, bufsize=1, text=True, env=env)
        self._thread = threading.Thread(target=self._run)
        self._thread.start()

        if self._detect_server_version:
            self._server_version = Version()
            PsgClient.send(self, {'jsonrpc': '2.0', 'method': 'raw', 'params': {'abs_path_ref': '/ADMIN/info'}, 'id': 'server_version'})

            for reply in PsgClient.receive(self):
                if result := reply.get('result', {}):
                    if result.get('reply') == 'unknown':
                        if version := result.get('data', {}).get('Version'):
                            self._server_version = Version(version)

            if not self._server_version:
                print("Warning: 'Failed to get server version'", file=sys.stderr)

        return self

    def _run(self):
        while self._pipe.poll() is None:
            try:
                if line := self._pipe.stdout.readline():
                    self._queue.put(line)
            except ValueError:
                break

    def __exit__(self, exc_type, exc_value, traceback):
        rv = exc_type == BrokenPipeError
        if not rv:
            self._pipe.stdin.close()
        self._pipe.stdout.close()
        try:
            self._pipe.terminate()
        except:
            pass
        self._thread.join()
        return rv

    def restart(self, env):
        if self._env != env:
            self._env = env
            self._pipe.stdin.close()
            self._pipe.stdout.close()
            self._pipe.wait()
            self._thread.join()
            self.__enter__();

    def send(self, request, id=None):
        print(json.dumps(request), file=self._pipe.stdin)

        if self._verbose >= PsgClient.VerboseLevel.REQUEST:
            print(json.dumps(request))

        self._sent[id or request['id']] = request
        return request

    def receive(self, /, deadline=Deadline()):
        while True:
            line = self._queue.get(timeout=deadline.get_remaining_or_raise()).rstrip()

            if self._verbose >= PsgClient.VerboseLevel.RESPONSE:
                print(line)

            try:
                data = json.loads(line)
            except json.decoder.JSONDecodeError:
                print(f"Error: 'Failed to parse response \"{line}\"'", file=sys.stderr)
                break

            yield data

            result = data.get('result')

            if not result or not result.get('reply'):
                return

    @property
    def server_version(self):
        return self._server_version

class SynthPsgClient(PsgClient):
    def __init__(self, args):
        self._request_generator = RequestGenerator()
        super().__init__(args)

    def send(self, method, **params):
        return super().send(*self._request_generator(method, **params))

    def receive(self, /, deadline=Deadline(), errors_only=False):
        for data in super().receive(deadline):
            if result := data.get('result'):
                status = result.get('status')

                # An item
                if reply_type := result.get('reply'):
                    # A normal result, yield
                    if status in (None, 'NotFound', 'Forbidden'):
                        if not errors_only:
                            yield result
                        continue

                # A successful reply, finish receiving
                elif status == 'Success':
                    return

                prefix = [reply_type or 'Reply', status]
                messages = result.get('errors', [])
                self._report_error(data, prefix, messages)
                yield result

                # An item
                if reply_type:
                    continue

            # A JSON-RPC error?
            result = data.get('error', {})
            code = result.get('code')
            prefix = ['Error', code] if code else ['Unknown reply']
            messages = [result.get('message', data)]
            self._report_error(data, prefix, messages)
            yield result
            return

    def _report_error(self, data, prefix, messages):
        request = json.dumps(self._sent[data.get('id')])
        print(*prefix, end=": '", file=sys.stderr)
        print(*messages, sep="', '", end=f"' for request '{request}'\n", file=sys.stderr)

def powerset(iterable):
    s = list(iterable)
    return itertools.chain.from_iterable(itertools.combinations(s, r) for r in range(len(s) + 1))

def syntest_all(psg_client, bio_ids, blob_ids, named_annots, chunk_ids, ipgs):
    blob_ids_only = list(blob_id[0] for v in blob_ids for blob_id in v.values())
    param_values = {
            'include_info': [None] + list(powerset((
                'all-info-except',
                'canonical-id',
                'name',
                'other-ids',
                'molecule-type',
                'length',
                'chain-state',
                'state',
                'blob-id',
                'tax-id',
                'hash',
                'date-changed',
                'gi'
            ))),
            'include_data': [
                None,
                'no-tse',
                'slim-tse',
                'smart-tse',
                'whole-tse',
                'orig-tse'
            ],
            'exclude_blobs': [None] + (list(random.choices(blob_ids_only, k=i) for i in range(4)) if blob_ids_only else []),
            'acc_substitution': [
                None,
                'default',
                'limited',
                'never'
            ],
            'bio_id_resolution': [
                None,
                False,
                True
            ],
            'snp_scale_limit': [
                None,
                'default',
                'unit',
                'contig',
                'supercontig',
                'chromosome'
            ],
            'resend_timeout': [
                None,
                0,
                100
            ],
            'user_args': [
                None,
                'mock',
                'bogus=fake'
            ] * 7,
        }

    def this_plus_random(param, value):
        """Yield this (param, value) pair plus pairs with random values for all other params."""
        for other in param_names:
            yield (other, value if other == param else random.choice(param_values[other]))

    # Used user_args to increase number of some requests
    requests = {
            'resolve':      [bio_ids,       ['include_info', 'acc_substitution', 'bio_id_resolution']],
            'biodata':      [bio_ids,       ['include_data', 'exclude_blobs', 'acc_substitution', 'bio_id_resolution', 'resend_timeout']],
            'blob':         [blob_ids,      ['include_data', 'user_args']],
            'named_annot':  [named_annots,  ['include_data', 'acc_substitution', 'bio_id_resolution', 'snp_scale_limit']],
            'chunk':        [chunk_ids,     ['user_args',    'user_args']],
            'acc_ver_history': [bio_ids,    ['user_args',    'user_args']],
            'ipg_resolve':  [ipgs,          ['user_args',    'user_args']]
        }

    rv = True
    summary = {}

    for method, (ids, param_names) in requests.items():
        n = 0
        for combination in (this_plus_random(param, value) for param in param_names for value in param_values[param]):
            try:
                random_ids=random.choice(ids)
            except IndexError:
                print(f"Error: 'No IDs for the \"{method}\" requests, skipping'", file=sys.stderr)
                rv = False
                break
            n += 1
            request = psg_client.send(method, **random_ids, **{p: v for p, v in combination if v is not None})
        summary[method] = n

    for method, n in summary.items():
        # The order is important, as we still need to receive everything
        rv = sum(len(list(psg_client.receive(errors_only=True))) for i in range(n)) == 0 and rv
        print(method, n, sep=': ')

    return rv

def get_ids(psg_client, bio_ids, /, max_blob_ids=1000, max_chunk_ids=1000):
    """Get corresponding blob and chunk IDs."""
    blob_ids = set()
    chunk_ids = set()

    for bio_id in bio_ids:
        psg_client.send('biodata', **bio_id, include_data='no-tse')
        for reply in psg_client.receive():
            item = reply.get('reply', None)

            if item == 'BlobInfo':
                blob_id = reply.get('id', {}).values()
                if blob_id:
                    blob_ids.add(tuple(blob_id))

                id2_info = reply.get('id2_info', '')
                if id2_info:
                    chunk = id2_info.split('.')[2]
                    if chunk:
                        for i in range(int(chunk)):
                            chunk_ids.add((i + 1, id2_info))
                        chunk_ids.add((999999999, id2_info))

        if len(blob_ids) >= max_blob_ids and len(chunk_ids) >= max_chunk_ids:
            break

    return [{'blob_id': blob_id} for blob_id in blob_ids], [{'chunk_id': chunk_id} for chunk_id in chunk_ids]

def get_all_named_annots(psg_client, named_annots, /, max_ids=1000):
    """Get all bio IDs for named annotations."""
    primary_types = {
             5: 'genbank',
             6: 'embl',
             7: 'pir',
             8: 'swissprot',
            10: 'other',
            13: 'ddbj',
            14: 'prf',
            15: 'pdb',
            16: 'tpg',
            17: 'tpe',
            18: 'tpd',
            # 19: 'gpipe', # Some gpipe IDs are not primary
        }

    ids = list()

    for (bio_id, na_ids) in named_annots:
        psg_client.send('named_annot', bio_id=[bio_id], named_annots=na_ids)
        for reply in psg_client.receive():
            item = reply.get('reply', None)

            if item == 'BioseqInfo':
                bio_ids = list()
                canonical_id = reply.get('canonical_id', {})
                if canonical_id:
                    bio_ids.append(list(canonical_id.values()))

                other_ids = reply.get('other_ids', [])
                for other_id in other_ids:
                    if other_id.get('type') in primary_types:
                        bio_ids.append(list(other_id.values()))

                ids.append([bio_ids, na_ids])

        if len(ids) >= max_ids:
            break

    return ids

def get_all_ipgs(psg_client, ipgs, /, max_ids=1000):
    """Get all IPGs."""
    ids = list()

    for id in ipgs:
        psg_client.send('ipg_resolve', **{k: v for k, v in zip(('protein', 'ipg', 'nucleotide'), id) if v is not None})
        for reply in psg_client.receive():
            item = reply.get('reply', None)

            if item == 'IpgInfo':
                protein = reply.get('protein', {})
                ipg_id = reply.get('ipg', {})
                nucleotide = reply.get('nucleotide', {})

                if protein or ipg_id:
                    ids.append([protein, ipg_id, nucleotide])

        if len(ids) >= max_ids:
            break

    return ids

def check_binary(args):
    for args.binary in (args.binary,) if args.binary else ('./psg_client', 'psg_client'):
        if shutil.which(args.binary):
            return
    else:
        sys.exit(f'"{args.binary}" does not exist or is not executable')

def read_bio_ids(input_file):
    return ([bio_id] + bio_type for (bio_id, *bio_type) in csv.reader(input_file))

def read_named_annots(input_file):
    return ([bio_id, na_ids] for (bio_id, *na_ids) in csv.reader(input_file))

def read_ipgs(input_file):
    return ([protein or None, int(ipg_id) if ipg_id else None] + nucleotide for (protein, ipg_id, *nucleotide) in csv.reader(input_file))

def prepare_bio_ids(bio_ids):
    """Get bio IDs in specific format."""
    return [{'bio_id': bio_id} for bio_id in bio_ids]

def prepare_named_annots(named_annots):
    """Get powerset of named annotations (excluding empty set) in specific format."""
    ids = list()

    for (bio_ids, na_ids) in named_annots:
        for na_subset in powerset(na_ids):
            if na_subset:
                for bio_subset in powerset(bio_ids):
                    if len(bio_subset) == 1:
                        ids.append({'bio_id': bio_subset[0], 'named_annots': na_subset})
                    elif len(bio_subset) > 1:
                        ids.append({'bio_ids': bio_subset, 'named_annots': na_subset})

    return ids

def prepare_ipgs(ipgs):
    """Get possible combinations of IPGs in specific format."""
    ids = {v for id in ipgs for v in itertools.compress(itertools.product(*zip(id, (None, None, None))), [1, 1, 1, 1, 0, 1])}
    return [{k: v for k, v in zip(('protein', 'ipg', 'nucleotide'), id) if v is not None} for id in ids]

def syntest_cmd(args):
    check_binary(args)

    if args.bio_file:
        bio_ids = list(read_bio_ids(args.bio_file))
    else:
        bio_ids = [
                ['emb|CQD33614.1'], ['emb|CQD24742.1'], ['emb|CQD05473.1'], ['emb|CQD25256.1'], ['emb|CPW37052.1'],
                ['emb|CPW38273.1'], ['emb|CPW43357.1'], ['emb|CPW37084.1'], ['emb|CQD02773.1'], ['emb|CQD03543.1'],
                ['emb|CQD06500.1'], ['emb|CQD06925.1'], ['emb|CRH17606.1'], ['emb|CRH18613.1'], ['emb|CRH28774.1'],
                ['emb|CRH31178.1'], ['emb|CRK75219.1'], ['emb|CRK74868.1'], ['emb|CRK82124.1'], ['emb|CRK85455.1'],
                ['emb|CRL52170.1'], ['emb|CRL52726.1'], ['emb|CRL57344.1'], ['emb|CRL57676.1'], ['emb|CRM10744.1'],
                ['emb|CRM09785.1'], ['emb|CRM17222.1'], ['emb|CRM17266.1'], ['emb|CQD30058.1'], ['emb|CQD27426.1'],
            ]

    if args.na_file:
        named_annots = list(read_named_annots(args.na_file))
    else:
        named_annots = [
                ['NC_000024', ['NA000000067.16', 'NA000134068.1', 'NA000000271.4']],
                ['NW_017890465', ['NA000122202.1', 'NA000122203.1']],
                ['NW_024096525', ['NA000288180.1']],
                ['NW_019824422', ['NA000150051.1']],
            ]

    if args.ipg_file:
        ipgs = list(read_ipgs(args.ipg_file))
    else:
        ipgs = [[None, 7093]]

    with SynthPsgClient(args) as psg_client:
        bio_ids = prepare_bio_ids(bio_ids)
        blob_ids, chunk_ids = get_ids(psg_client, bio_ids)
        named_annots = get_all_named_annots(psg_client, named_annots)
        named_annots = prepare_named_annots(named_annots)
        ipgs = get_all_ipgs(psg_client, ipgs)
        ipgs = prepare_ipgs(ipgs)
        result = syntest_all(psg_client, bio_ids, blob_ids, named_annots, chunk_ids, ipgs)
        sys.exit(0 if result else -1)

class Response(collections.UserList):
    """PSG replies optionally sorted by their JSON representations."""

    def __init__(self, iterable, /, actual=False):
        super().__init__(sorted(iterable, key=json.dumps) if actual else iterable)

    def apply(self, expected, deadline):
        """Apply null from the expected response."""

        def _any(actual_any, index, expected, modify):
            actual = actual_any[index]
            if isinstance(actual, dict):
                if isinstance(expected, dict):
                    return _dict(actual, expected, modify)
            elif isinstance(actual, list):
                if isinstance(expected, list):
                    return _list(actual, expected, modify)
            elif isinstance(actual, str) and isinstance(expected, dict) and 'regex' in expected:
                if re.fullmatch(expected['regex'], actual):
                    if modify:
                        actual_any[index] = expected
                    return True

                return False
            else:
                return actual == expected

            return False

        def _list(actual, expected, modify, deadline=Deadline()):
            i = 0
            success = True
            matching_any = False

            for e in expected:
                deadline.get_remaining_or_raise()

                if e is None:
                    matching_any = True
                    continue

                for k, a in enumerate(itertools.islice(actual, i, None)):
                    if _any(actual, i + k, e, modify=False):
                        # Modify only after it fully matches
                        if modify:
                            _any(actual, i + k, e, modify=True)

                            if matching_any:
                                actual[i:i + k] = [None]

                        if k > 0:
                            if not matching_any:
                                # Found after some unexpected values
                                success = False
                            elif modify:
                                i += 1
                            else:
                                i += k

                        i += 1
                        matching_any = False
                        break
                else:
                    if not matching_any:
                        # Missing this expected value
                        success = False
                    elif modify:
                        actual[i] = None
                    else:
                        i += 1

            if not matching_any:
                if i < len(actual):
                    success = False
            elif modify:
                actual[i:] = [None]

            return success

        def _dict(actual, expected, modify):
            if actual.keys() != expected.keys():
                return False

            return all(map(lambda args: _dict_elem(*args), ((actual, k, expected[k], modify) for k in actual.keys())))

        def _dict_elem(actual, i, expected, modify):
            if expected is not None:
                return _any(actual, i, expected, modify)

            if modify:
                actual[i] = None

            return True

        _list(self.data, expected.data, modify=True, deadline=deadline)

    def diff(self, other, keepends):
        """Produce a unified diff between two responses."""

        def _to_lines(iterable):
            """Transform a response into JSON and split it into list of strings."""
            return list(itertools.chain.from_iterable((json.dumps(value, indent=2) + '\n').splitlines(keepends=keepends) for value in iterable))

        return list(unified_diff(_to_lines(self), _to_lines(other), fromfile='Expected', tofile='Actual', n=3, lineterm='\n' if keepends else ''))

class Processor:
    class Run:
        def __init__(self, name, data, parent_data=None):
            if parent_data is None:
                parent_data = {}

            self._name = name
            self._deadline = Deadline(data.get('timeout')) | Deadline(parent_data.get('timeout'))
            self._env = data.get('env', {}) | parent_data.get('env', {})
            self._user_args = '&'.join(filter(None, [data.get('user_args'), parent_data.get('user_args')]))
            self._exclude = data.get('exclude', []) + parent_data.get('exclude', [])
            self._require = data.get('require', []) + parent_data.get('require', [])
            self._filter = ') and ('.join(filter(None, [data.get('filter', ''), parent_data.get('filter', '')]))
            self._filter = f'({self._filter})' if self._filter else ''

        def adjust(self, deadline):
            self._deadline |= deadline

        @property
        def name(self):
            return self._name

        @property
        def deadline(self):
            return self._deadline

        @property
        def env(self):
            return self._env

        @property
        def user_args(self):
            return self._user_args

        @property
        def exclude(self):
            return self._exclude

        @property
        def require(self):
            return self._require

        @property
        def filter(self):
            return self._filter

    class Testcase:
        def __init__(self, run, testcase):
            self._run = run
            self._testcase = testcase
            self._actual = []
            self._deadline = Deadline(self.data.get('timeout'))
            self._env = self.data.get('env', {}).copy()
            self._skipped = False

        def __iadd__(self, replies):
            self._actual.extend(replies)
            return self

        def __str__(self):
            return '\n\t'.join([':'] + self.namedesc + [''])

        def __call__(self):
            expected = Response(self.data['response'])
            actual = Response(self._actual, actual=True)
            actual.apply(expected, deadline=self.deadline)
            return expected, actual

        def adjust(self, run):
            self._deadline |= run.deadline
            env = run.env.copy()

            if user_args := run.user_args:
                user_args_param = 'NCBI_CONFIG__PSG__REQUEST_USER_ARGS'

                # Do not override NCBI_CONFIG__PSG__REQUEST_USER_ARGS, add to it
                run_env_user_args = env.get(user_args_param)
                testcase_env_user_args = self._env.get(user_args_param)
                user_args = '&'.join(filter(None, (run_env_user_args, testcase_env_user_args, user_args)))
                env[user_args_param] = user_args

            self._env.update(env)

        def skip(self, server_version):
            if server_version:
                if requirements := self.data.get('server', []):
                    condition_ops = { "min-ver": operator.ge, "max-ver": operator.le }

                    for requirement in requirements:
                        for condition, value in requirement.items():
                            if op := condition_ops.get(condition):
                                if not op(server_version, Version(value)):
                                    break
                            else:
                                print(f"Warning: 'Unknown version condition \"{condition}\"'", file=sys.stderr)
                        else:
                            return False

                    self._skipped = True
                    return True

            return False

        @property
        def name(self):
            return self._testcase[0]

        @property
        def namedesc(self):
            namedesc = ['.'.join(filter(None, (self._run, self.name)))]

            if desc := self.data.get('description'):
                if desc != self.name:
                    namedesc.append(desc)

            return namedesc

        @property
        def data(self):
            return self._testcase[1]

        @property
        def deadline(self):
            return self._deadline

        @property
        def env(self):
            return self._env

        @property
        def skipped(self):
            return self._skipped

    def __init__(self):
        self._total = 0
        self._data = json.loads(''.join(filter(lambda l: l.lstrip()[:1] not in ['#', ''], args.input_file)))

        if args.json_validate:
            self._validate()

    def _validate(self):
        result = subprocess.run([args.binary, 'interactive_schema'], text=True, capture_output=True)
        schema = json.loads(result.stdout)

        # Remove old body
        del schema['oneOf']

        # Add definitions, overwrite anything else
        for key, value in testcases_schema_addon().items():
            if key == 'definitions':
                schema[key].update(value)
            else:
                schema[key] = value

        with tempfile.NamedTemporaryFile(mode='w+') as schema_file:
            json.dump(schema, schema_file)
            schema_file.seek(0)
            cmd = [args.binary, 'json_check', '-schema-file', schema_file.name, '-single-doc']

            try:
                subprocess.run(cmd, input=json.dumps(self._data), check=True, text=True, capture_output=True)
            except subprocess.CalledProcessError as e:
                sys.exit(f'"{args.input_file.name}" failed JSON schema validation:\n{e.stdout}')

    def _get_cond_filter(self, /, require=True):
        return lambda tags, cond: not (require or tags) or not cond or eval(cond)

    def get_runs(self):
        def _should_run(name):
            if args.run:
                return name in args.run
            elif args.no_run:
                return name not in args.no_run
            else:
                return name == ''

        runs = self.data.get('runs', {})

        for name in sorted(filter(_should_run, set(self.data.get('runs', {}).keys()) | set(('',)))):
            run_data = runs.get(name)

            if run_data is None:
                sys.exit(f'Cannot find run "{name}"')
            elif sub_runs := run_data.get('sub-runs'):
                for simple_name in sorted(sub_runs):
                    yield self.Run(simple_name, runs.get(simple_name), parent_data=run_data)
            else:
                yield self.Run(name, run_data)

    def get_testcases(self, run):
        exclude = run.exclude
        require = run.require

        exclude_filter = '!' + ' and !'.join(exclude) if exclude else ''
        require_filter = ' or '.join(require) if require else ''

        if exclude and require:
            run_filter = f'({exclude_filter}) and ({require_filter})'
        elif exclude:
            run_filter = exclude_filter
        elif require:
            run_filter = require_filter
        else:
            run_filter = run.filter

        replaces = (
                (r'\bnot\s',   r'!'),
                (r'\band\b',   r'&&'),
                (r'\bor\b',    r'||'),
                (r'(!?)([\w-]+)', r'"\2"\1 in tags'),
                (r'!',         r' not'),
                (r'&&',        r'and'),
                (r'\|\|',      r'or')
            )
        cond = functools.reduce(lambda c, r: re.sub(*r, c, flags=re.IGNORECASE), replaces, run_filter)
        cond_filter = self._get_cond_filter(require)

        def testcases_filter(testcase):
            name, testcase = testcase

            if args.testcase:
                return name in args.testcase
            elif args.no_testcase:
                return name not in args.no_testcase
            else:
                tags = set(testcase.get('tags', []))

                if args.tag:
                    return tags & args.tag
                elif args.no_tag:
                    return not tags & args.no_tag
                else:
                    return cond_filter(tags, cond)

        testcases = self.data.get('testcases')

        for testcase in filter(testcases_filter, testcases.items()):
            yield testcase

    @contextmanager
    def __call__(self, run, testcase):
        self._total += 1
        self._testcase = self.Testcase(run, testcase)

        try:
            yield self.testcase_to_run

            if self._testcase.skipped:
                self._total -= 1
            else:
                self._complete()
        except queue.Empty:
            self._timeout()

    @property
    def data(self):
        return self._data

    @property
    def testcase_to_run(self):
        return self._testcase

    @property
    def deadline(self):
        return Deadline(self._data.get('timeout', 25))

    @staticmethod
    @contextmanager
    def create(args):
        if args.output_file:
            processor = OutputFileProcessor()
        elif args.list:
            processor = ListProcessor()
        elif args.report == 'text':
            processor = ReportTextProcessor()
        elif args.report == 'json':
            processor = ReportJsonProcessor()
        else:
            processor = ReportProcessor()
        try:
            yield processor
            processor._close()
        except KeyboardInterrupt:
            pass

class OutputFileProcessor(Processor):
    def __init__(self):
        self._updated_testcases = {}
        super().__init__()

    def _get_cond_filter(self, /, require=True):
        if args.run or args.no_run:
            return super()._get_cond_filter(require)
        else:
            return lambda tags, cond: True

    def _complete(self):
        expected, actual = self._testcase()
        self._testcase.data['response'] = actual.data
        self._updated_testcases[self._testcase.name] = self._testcase.data

    def _timeout(self):
        print(f"Error: 'Testcase \"{self._testcase.name}\" timed out'", file=sys.stderr)
        sys.exit(2)

    def _close(self):
        self._data['testcases'].update(self._updated_testcases)
        join = False

        with open(args.output_file, 'w') as of:
            for line in json.dumps(self.data, indent=4).splitlines():
                if re.fullmatch(r'\s+"testcases": {', line):
                    print(file=of)
                if re.fullmatch(r'\s+"(env|exclude|require|sub-runs|tags)": [\[{]', line):
                    print(line, end='', file=of)
                    join = True
                elif not join:
                    print(line, file=of)
                elif re.fullmatch(r'\s+[\]}],?', line):
                    print(line.lstrip(), file=of)
                    join = False
                else:
                    print(re.sub(r',$', ', ', line.strip()), end='', file=of)
            of.write('\n')

    @property
    def deadline(self):
        return Deadline(600)

class ListProcessor(Processor):
    @property
    def testcase_to_run(self):
        return None

    def _complete(self):
        print('Testcase' + str(self._testcase))

    def _close(self):
        print(f'{self._total} testcases total')

class ReportBaseProcessor(Processor):
    def __init__(self):
        self._passed = 0
        super().__init__()

    def _diff(self, keepends):
        expected, actual = self._testcase()
        diff = expected.diff(actual, keepends=keepends)

        if not diff:
            self._passed += 1

        return diff

class ReportJsonProcessor(ReportBaseProcessor):
    class Testcase(ReportBaseProcessor.Testcase):
        def __str__(self):
            return ' - '. join(self.namedesc)

    def __init__(self):
        self._output = {}
        super().__init__()

    def _complete(self):
        if diff := self._diff(keepends=False):
            self._output.setdefault('failed', {})[str(self._testcase)] = diff
        else:
            self._output.setdefault('passed', []).append(str(self._testcase))

    def _timeout(self):
        self._output.setdefault('timeout', []).append(str(self._testcase))

    def _close(self):
        print(json.dumps(self._output, indent=2))

        if self._passed != self._total:
            sys.exit(1)

class ReportProcessor(ReportBaseProcessor):
    def _complete(self):
        if diff := super()._diff(keepends=True):
            print('Testcase failed' + str(self._testcase), end='')
            print(*diff, sep='\t')

        return not bool(diff)

    def _timeout(self):
        print('Testcase timed out' + str(self._testcase))
        sys.exit(2)

    def _close(self):
        if self._passed != self._total:
            sys.exit(1)

class ReportTextProcessor(ReportProcessor):
    def _complete(self):
        if super()._complete():
            print('Testcase passed' + str(self._testcase))

    def _close(self):
        if not self._total:
            print('No testcases run')
        elif self._passed == self._total:
            print(f'All {self._total} testcases passed')
        else:
            print(f'{self._passed} of {self._total} testcases passed')

        super()._close()

def testcases_cmd(args):
    check_binary(args)
    args.tag = set(args.tag or [])
    args.no_tag = set(args.no_tag or [])
    args.run = set(args.run or [])
    args.no_run = set(args.no_run or [])

    with Processor.create(args) as processor, PsgClient(args) as psg_client:
        overall_deadline = processor.deadline if args.timeout is None else Deadline(args.timeout)

        for run in processor.get_runs():
            run.adjust(overall_deadline)

            for testcase in processor.get_testcases(run):
                with processor(run.name, testcase) as testcase:
                    if testcase:
                        testcase.adjust(run)
                        testcase.deadline.get_remaining_or_raise()

                        psg_client.restart(testcase.env)

                        if testcase.skip(psg_client.server_version):
                            continue

                        for request_batch in testcase.data['requests']:
                            for i, request in enumerate(request_batch):
                                psg_client.send(request)

                            for i in range(i + 1):
                                testcase += psg_client.receive(deadline=testcase.deadline)

def generate_cmd(args):
    check_binary(args)
    request_generator = RequestGenerator()

    if args.TYPE == 'named_annot':
        named_annots = read_named_annots(args.INPUT_FILE)

        with SynthPsgClient(args) as psg_client:
            named_annots = get_all_named_annots(psg_client, named_annots, max_ids=args.NUMBER)

        ids = prepare_named_annots(named_annots)
    elif args.TYPE == 'ipg_resolve':
        ipgs = read_ipgs(args.INPUT_FILE)

        with SynthPsgClient(args) as psg_client:
            ipgs = get_all_ipgs(psg_client, ipgs, max_ids=args.NUMBER)

        ids = prepare_ipgs(ipgs)
    else:
        bio_ids = prepare_bio_ids(read_bio_ids(args.INPUT_FILE))

        if args.TYPE in ('acc_ver_history', 'biodata', 'resolve'):
            ids = bio_ids
        elif args.TYPE in ('blob', 'chunk'):
            max_blob_ids = args.NUMBER if args.TYPE == 'blob' else 0
            max_chunk_ids = args.NUMBER if args.TYPE != 'blob' else 0

            with SynthPsgClient(args) as psg_client:
                blob_ids, chunk_ids = get_ids(psg_client, bio_ids, max_blob_ids=max_blob_ids, max_chunk_ids=max_chunk_ids)

            ids = blob_ids if args.TYPE == 'blob' else chunk_ids

    ids = itertools.cycle(ids)
    params = {} if args.params is None else ast.literal_eval(args.params)

    try:
        for i in range(args.NUMBER):
            print(json.dumps(request_generator(args.TYPE, **next(ids), **params)[0]))
    except StopIteration:
        sys.exit(f'"{args.INPUT_FILE.name}" has no [appropriate] IDs to generate {args.TYPE} requests')

def cgi_help_cmd(args):
    class Params(dict):
        def add(self, param_name, param_type, param_desc, req_name):
            r = self.setdefault((param_name, param_desc), {'type': param_type})
            r.setdefault('requests', []).append(req_name)

    class Requests(dict):
        def add(self, req_name, req_desc, require, exclude):
            self.setdefault(req_name, {}).update({'description': req_desc, 'require': require, 'exclude': exclude})

        def param(self, req_name, param_name, param_desc):
            r = self.setdefault(req_name, {})
            r.setdefault('params', []).append((param_name, param_desc))

    ns = {'ns': 'ncbi:application'}
    show_requests = ['acc_ver_history', 'biodata', 'blob', 'chunk', 'ipg_resolve', 'named_annot', 'resolve']
    hide_keys = ['conffile', 'debug-printout', 'input-file', 'io-threads', 'logfile', 'max-streams', 'requests-per-io', 'worker-threads', 'rate', 'service']
    hide_flags = ['dryrun', 'latency', 'all-latency', 'first-latency', 'last-latency', 'server-mode']
    tse_flags = ['no-tse', 'slim-tse', 'smart-tse', 'whole-tse', 'orig-tse']
    info_flags = ['canonical-id', 'name', 'other-ids', 'molecule-type', 'length', 'chain-state', 'state', 'blob-id', 'tax-id', 'hash', 'date-changed', 'gi', 'all-info-except']

    check_binary(args)
    result = subprocess.run([args.binary, '-xmlhelp'], text=True, capture_output=True)
    parsed = ET.fromstring(result.stdout)

    params = Params()
    requests = Requests()

    for request in parsed.iterfind('ns:command', ns):
        req_name = request.find('ns:name', ns).text

        if req_name not in show_requests:
            continue

        req_desc = request.find('ns:description', ns).text

        for arg in request.iterfind('ns:arguments', ns):
            require = {}
            exclude = {}

            for dep in arg.iterfind('ns:dependencies', ns):
                for data in dep.iterfind('ns:first_requires_second', ns):
                    arg1 = data.find('ns:arg1', ns).text
                    arg2 = data.find('ns:arg2', ns).text
                    require.setdefault(arg1, {}).setdefault(arg2, None)

                for data in dep.iterfind('ns:first_excludes_second', ns):
                    arg1 = data.find('ns:arg1', ns).text
                    arg2 = data.find('ns:arg2', ns).text
                    exclude.setdefault(arg1, {}).setdefault(arg2, None)

            require = {name:list(data.keys()) for name, data in require.items()}
            exclude = {name: [v for v in data.keys() if v not in hide_keys] for name, data in exclude.items()}
            requests.add(req_name, req_desc, require, exclude)

            for pos in arg.iterfind('ns:positional', ns):
                pos_name = pos.get('name').lower()
                pos_desc = pos.find('ns:description', ns).text
                params.add(pos_name, 'mandatory', pos_desc, req_name)
                requests.param(req_name, pos_name, pos_desc)

            for extra in arg.iterfind('ns:extra', ns):
                extra_name = 'id'
                extra_optional = extra.get('optional')
                extra_type = 'optional' if extra_optional == 'true' else 'mandatory'
                extra_desc = extra.find('ns:description', ns).text
                params.add(extra_name, extra_type, extra_desc, req_name)
                requests.param(req_name, extra_name, extra_desc)

            for key in arg.iterfind('ns:key', ns):
                key_name = key.get('name')
                key_optional = key.get('optional')

                if key_name in hide_keys:
                    continue

                values = []
                constraint = key.find('ns:constraint', ns)
                if constraint is not None:
                    for val in constraint.find('ns:Strings', ns).iterfind('ns:value', ns):
                        values.append(val.text)
                values = ', (accepts: ' + '|'.join(values) + ')' if values else ''

                default = key.find('ns:default', ns)
                default = '' if default is None else f', (default: {default.text})'

                key_type = 'optional' if key_optional == 'true' else 'mandatory'
                key_desc = key.find('ns:description', ns).text + values + default
                params.add(key_name, key_type, key_desc, req_name)
                requests.param(req_name, key_name, key_desc)

            req_flags = {}

            for flag in arg.iterfind('ns:flag', ns):
                flag_name = flag.get('name')

                if flag_name in hide_flags:
                    continue
                elif flag_name in tse_flags:
                    flag_type = 'TSE flags'
                elif flag_name in info_flags:
                    flag_type = 'info flags'
                else:
                    flag_type = 'flags'

                flag_desc = flag.find('ns:description', ns).text
                params.add(flag_name, flag_type, flag_desc, req_name)
                req_flags.setdefault(flag_type, []).append([flag_name, flag_desc])

            # Add flag groups in particular order
            for flags in ['flags', 'info flags', 'TSE flags']:
                for req_flag in req_flags.get(flags, []):
                    requests.param(req_name, *req_flag)

    result_reqs = {}
    common_params = {}

    for req_name, req_data in requests.items():
        result_data = {'description': req_data['description']}
        result_req_data = result_reqs.setdefault(req_name, {})
        result_req_data['description'] = req_data['description']

        for param in req_data['params']:
            param_data = params[param]
            param_name, param_desc = param
            param_type = param_data['type']

            if param_data['requests'] == show_requests:
                data = common_params.setdefault(param_type, {})
            else:
                data = result_req_data.setdefault('Request-specific Parameters', {}).setdefault(param_type, {})

                # Add specific flags together, in particular order
                for specific_flags in [info_flags, tse_flags]:
                    if param_name in specific_flags:
                        if data.get(param_name) is None:
                            data.update({name: '' for name in specific_flags})

                require = req_data['require'].get(param_name)
                if require:
                    param_desc += ', (requires: ' + ','.join(require) + ')'

                exclude = req_data['exclude'].get(param_name)
                if exclude:
                    param_desc += ', (excludes: ' + ','.join(exclude) + ')'

            data[param_name] = param_desc

    # Update description for resolve (add POST batch-resolve)
    get_resolve = result_reqs.pop('resolve')
    resolve_params = get_resolve.pop('Request-specific Parameters')
    resolve_mandatory = resolve_params.pop('mandatory')
    post_resolve = get_resolve.copy()
    get_resolve['Method-specific Parameters'] = {'mandatory': resolve_mandatory}
    post_resolve['description'] = 'Batch request biodata info by bio IDs'
    post_resolve['Method-specific Parameters'] = {'mandatory': {'List of bio IDs': 'One bio ID per line in the request body'}}

    # Update description for IPG resolve (add POST IPG batch-resolve)
    get_ipg_resolve = result_reqs.pop('ipg_resolve')
    ipg_resolve_params = get_ipg_resolve.pop('Request-specific Parameters')
    ipg_resolve_optional = ipg_resolve_params.pop('optional')
    post_ipg_resolve = get_ipg_resolve.copy()
    get_ipg_resolve['Method-specific Parameters'] = {'optional': ipg_resolve_optional}
    post_ipg_resolve['description'] = 'Batch request IPG info by protein and, optionally, by nucleotide'
    post_ipg_resolve['Method-specific Parameters'] = {'mandatory': {'List of IDs': 'One \'protein[,nucleotide]\' per line in the request body'}}

    result_reqs['resolve'] = {'method': {'GET': get_resolve, 'POST': post_resolve}, 'Request-specific Parameters': resolve_params}
    result_reqs['ipg_resolve'] = {'method': {'GET': get_ipg_resolve, 'POST': post_ipg_resolve} }
    result = {'request': result_reqs, 'Common Parameters': common_params}
    json.dump(result, args.output_file, indent=2)

def log2json_cmd(args):
    match = functools.partial(re.match, r'[^/]+:\d+(/(ID|IPG)/\w+\?[^;]+)')

    for i, m in enumerate(filter(None, map(match, sys.stdin)), start=1):
        print('{"jsonrpc": "2.0", "method": "raw", "params": {"abs_path_ref": "' + m[1] + '"}, "id": "raw_' + str(i) + '"}')

def testcases_schema_addon():
    return {
            'definitions': {
                'env': {
                    '$id': '#env',
                    'type': 'object'
                },
                'pattern': {
                    '$id': '#pattern',
                    'type': 'object',
                    'properties': {
                        'regex': {
                            'type': 'string'
                        }
                    },
                    'required': [
                        'regex'
                    ]
                },
                'message': {
                    '$id': '#message',
                    'properties': {
                        'severity': {
                            'type': 'string'
                        },
                        'code': {
                            'type': 'number'
                        },
                        'text': {
                            'oneOf': [
                                {
                                    'type': 'string'
                                },
                                {
                                    '$ref': '#/definitions/pattern'
                                }
                            ]
                        },
                    },
                    'additionalProperties': False,
                    'required': [
                        'severity',
                        'code',
                        'text'
                    ]
                },
                'messages': {
                    '$id': '#messages',
                    'type': 'array',
                    'items': {
                        '$ref': '#/definitions/message'
                    }
                },
                'reply': {
                    '$id': '#reply',
                    'type': 'object',
                    'properties': {
                        'status': {
                            'type': 'string'
                        },
                        'messages': {
                            '$ref': '#/definitions/messages'
                        }
                    },
                    'additionalProperties': False,
                    'required': [
                        'status'
                    ]
                },
                'reply_item': {
                    '$id': '#reply_item',
                    'type': 'object',
                    'properties': {
                        'reply': {
                            'type': 'string'
                        },
                        'status': {
                            'type': 'string'
                        },
                        'messages': {
                            '$ref': '#/definitions/messages'
                        }
                    },
                    'required': [
                        'reply'
                    ]
                },
                'requests': {
                    '$id': '#requests',
                    'type': 'array',
                    'items': {
                        'type': 'array',
                        'items': {
                            'oneOf': [
                                {
                                    '$ref': '#/definitions/biodata'
                                },
                                {
                                    '$ref': '#/definitions/blob'
                                },
                                {
                                    '$ref': '#/definitions/resolve'
                                },
                                {
                                    '$ref': '#/definitions/named_annot'
                                },
                                {
                                    '$ref': '#/definitions/chunk'
                                },
                                {
                                    '$ref': '#/definitions/ipg_resolve'
                                },
                                {
                                    '$ref': '#/definitions/raw'
                                }
                            ]
                        }
                    }
                },
                'response': {
                    '$id': '#response',
                    'type': 'array',
                    'items': {
                        'oneOf': [
                            {
                                '$ref': '#/definitions/response_item'
                            },
                            {
                                'type': 'null'
                            }
                        ]
                    }
                },
                'response_item': {
                    '$id': '#response_item',
                    'type': 'object',
                    'properties': {
                        'jsonrpc': {
                            '$ref': '#/definitions/jsonrpc'
                        },
                        'id': {
                            '$ref': '#/definitions/id'
                        },
                        'result': {
                            'oneOf': [
                                {
                                    '$ref': '#/definitions/reply_item'
                                },
                                {
                                    '$ref': '#/definitions/reply'
                                },
                                {
                                    'type': 'null'
                                }
                            ]
                        }
                    },
                    'required': [
                        'jsonrpc',
                        'id',
                        'result'
                    ]
                },
                'run': {
                    '$id': '#run',
                    'type': 'object',
                    'properties': {
                        'env': {
                            '$ref': '#/definitions/env'
                        },
                        'exclude': {
                            '$ref': '#/definitions/tags'
                        },
                        'require': {
                            '$ref': '#/definitions/tags'
                        },
                        'sub-runs': {
                            '$ref': '#/definitions/sub_runs'
                        },
                        'timeout': {
                            '$ref': '#/definitions/timeout'
                        },
                        'user_args': {
                            '$ref': '#/definitions/user_args'
                        }
                    },
                    'additionalProperties': False
                },
                'sub_runs': {
                    '$id': '#sub_runs',
                    'type': 'array',
                    'items': {
                        'type': 'string'
                    }
                },
                'tags': {
                    '$id': '#tags',
                    'type': 'array',
                    'items': {
                        'type': 'string'
                    }
                },
                'testcase': {
                    '$id': '#testcase',
                    'type': 'object',
                    'properties': {
                        'description': {
                            'type': 'string'
                        },
                        'env': {
                            '$ref': '#/definitions/env'
                        },
                        'requests': {
                            '$ref': '#/definitions/requests'
                        },
                        'response': {
                            '$ref': '#/definitions/response'
                        },
                        'tags': {
                            '$ref': '#/definitions/tags'
                        },
                        'timeout': {
                            '$ref': '#/definitions/timeout'
                        }
                    },
                    'additionalProperties': False,
                    'required': [
                        'requests'
                    ]
                },
                'timeout': {
                    '$id': '#timeout',
                    'type': 'number'
                }
            },
            'properties': {
                'runs': {
                    'type': 'object',
                    'patternProperties': {
                        '.*' : {
                            '$ref': '#/definitions/run'
                        }
                    }
                },
                'testcases': {
                    'type': 'object',
                    'patternProperties': {
                        '.*' : {
                            '$ref': '#/definitions/testcase'
                        }
                    }
                }
            },
            'additionalProperties': False
        }

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(title='available commands', metavar='COMMAND', required=True, dest='command')

    parser_syntest = subparsers.add_parser('syntest', help='Perform synthetic psg_client tests', description='Perform synthetic psg_client tests', aliases=['test'])
    parser_syntest.set_defaults(func=syntest_cmd)
    parser_syntest.add_argument('-service', help='PSG service/server to use')
    parser_syntest.add_argument('-https', help='Enable HTTPS', action='store_true')
    parser_syntest.add_argument('-binary', help='psg_client binary to run (default: tries ./psg_client, then $PATH/psg_client)')
    parser_syntest.add_argument('-bio-file', help='CSV file with bio IDs "BioID[,Type]" (default: some hard-coded IDs)', type=argparse.FileType())
    parser_syntest.add_argument('-na-file', help='CSV file with named annotations "BioID,NamedAnnotID[,NamedAnnotID]..." (default: some hard-coded IDs)', type=argparse.FileType())
    parser_syntest.add_argument('-ipg-file', help='CSV file with IPG IDs "Protein,[IPG][,Nucleotide]" (default: some hard-coded IDs)', type=argparse.FileType())
    parser_syntest.add_argument('-verbose', '-v', help='Verbose output (multiple are allowed)', action='count', default=0)
    parser_syntest.add_argument('-no-testing-opt', help='Do not pass option "-testing" to psg_client binary', dest='testing', action='store_false')

    parser_testcases = subparsers.add_parser('testcases', help='Perform JSON-specified PSG tests', description='Perform JSON-specified PSG tests')
    parser_testcases.set_defaults(func=testcases_cmd)
    group1_testcases = parser_testcases.add_mutually_exclusive_group()
    group2_testcases = parser_testcases.add_mutually_exclusive_group()
    parser_testcases.add_argument('-service', help='PSG service/server to use')
    parser_testcases.add_argument('-https', help='Enable HTTPS', action='store_true')
    parser_testcases.add_argument('-binary', help='psg_client binary to run (default: tries ./psg_client, then $PATH/psg_client)')
    parser_testcases.add_argument('-input-file', help='JSON file with testcases" (default: ./testcases.json)', metavar='FILE', type=argparse.FileType(), default='./testcases.json')
    group1_testcases.add_argument('-output-file', help='Output updated testcases file (e.g. if they need updating)', metavar='FILE')
    group2_testcases.add_argument('-testcase', help='Run only specified testcase(s) (multiple are allowed)', action='append')
    group2_testcases.add_argument('-no-testcase', help='Run all but specified testcase(s) (multiple are allowed)', action='append')
    group2_testcases.add_argument('-tag', help='Run only specified tag(s) (multiple are allowed)', action='append')
    group2_testcases.add_argument('-no-tag', help='Run all but specified tag(s) (multiple are allowed)', action='append')
    group2_testcases.add_argument('-run', help='Run only specified run(s) (multiple are allowed)', action='append')
    group2_testcases.add_argument('-no-run', help='Run all but specified run(s) (multiple are allowed)', action='append')
    group1_testcases.add_argument('-report', help='Report testcase results', choices=['text', 'json'], nargs='?', const='text')
    group1_testcases.add_argument('-list', help='List available testcases', action='store_true')
    parser_testcases.add_argument('-data-limit', help='Show a data preview for any data larger the limit (if set)', default='1KB')
    parser_testcases.add_argument('-preview-size', help='How much of data to show as the preview', default='16')
    parser_testcases.add_argument('-timeout', help='Overall timeout (overrides overall timeout from JSON file)', type=int)
    parser_testcases.add_argument('-server-version', help='Overrides detected server version (if set empty, disables version filtering)')
    parser_testcases.add_argument('-verbose', '-v', help='Verbose output (multiple are allowed)', action='count', default=0)
    parser_testcases.add_argument('-no-one-server-opt', help=argparse.SUPPRESS, dest='one_server', action='store_false')
    parser_testcases.add_argument('-no-testing-opt', help=argparse.SUPPRESS, dest='testing', action='store_false')
    parser_testcases.add_argument('-no-json-validate', help=argparse.SUPPRESS, dest='json_validate', action='store_false')

    parser_generate = subparsers.add_parser('generate', help='Generate JSON-RPC requests for psg_client', description='Generate JSON-RPC requests for psg_client')
    parser_generate.set_defaults(func=generate_cmd)
    parser_generate.add_argument('-binary', help='psg_client binary to run (default: tries ./psg_client, then $PATH/psg_client)')
    parser_generate.add_argument('-params', help='Params to add to requests (e.g. \'{"include_info": ["canonical-id", "gi"]}\')')
    parser_generate.add_argument('-verbose', '-v', help='Verbose output (multiple are allowed)', action='count', default=0)
    parser_generate.add_argument('INPUT_FILE', help='CSV file with bio IDs "BioID[,Type]", named annotations "BioID,NamedAnnotID[,NamedAnnotID]... or IPG IDs "Protein,[IPG][,Nucleotide]"', type=argparse.FileType())
    parser_generate.add_argument('TYPE', help='Type of requests (resolve, biodata, blob, named_annot, chunk, acc_ver_history or ipg_resolve)', metavar='TYPE', choices=['resolve', 'biodata', 'blob', 'named_annot', 'chunk', 'acc_ver_history', 'ipg_resolve'])
    parser_generate.add_argument('NUMBER', help='Max number of requests', type=int)

    parser_cgi_help = subparsers.add_parser('cgi_help', help='Generate JSON help for pubseq_gateway.cgi (by parsing psg_client help)', description='Generate JSON help for pubseq_gateway.cgi (by parsing psg_client help)')
    parser_cgi_help.set_defaults(func=cgi_help_cmd)
    parser_cgi_help.add_argument('-binary', help='psg_client binary to run (default: tries ./psg_client, then $PATH/psg_client)')
    parser_cgi_help.add_argument('-output-file', help='Output file (default: %(default)s)', metavar='FILE', default='-', type=argparse.FileType('w'))

    parser_log2json = subparsers.add_parser('log2json', help='Create JSON-RPC requests from parsed debug printout', description='Create JSON-RPC requests from parsed debug printout')
    parser_log2json.set_defaults(func=log2json_cmd)

    args = parser.parse_args()
    args.func(args)
