#!/usr/bin/env python3

import argparse
import ast
import csv
import collections
from difflib import unified_diff
import enum
import functools
import itertools
import json
import queue
import random
import shutil
import subprocess
import sys
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

class PsgClient:
    VerboseLevel = enum.IntEnum('VerboseLevel', 'REQUEST RESPONSE DEBUG')

    def __init__(self, args):
        self._cmd = [args.binary, 'interactive', '-server-mode']
        self._verbose = args.verbose
        self._sent = {}

        args_dict = vars(args)

        if service := args_dict.get('service'):
            self._cmd += ['-service', service]

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

    def __enter__(self):
        self._pipe = subprocess.Popen(self._cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, bufsize=1, text=True)
        self._queue = queue.Queue()
        self._thread = threading.Thread(target=self._run)
        self._thread.start()
        return self

    def _run(self):
        while self._pipe.poll() is None:
            try:
                line = self._pipe.stdout.readline()
            except ValueError:
                break
            else:
                self._queue.put(line)

    def __exit__(self, exc_type, exc_value, traceback):
        rv = exc_type == BrokenPipeError
        if not rv:
            self._pipe.stdin.close()
        self._pipe.stdout.close()
        try:
            self._pipe.terminate()
        finally:
            self._thread.join()
        return rv

    def send(self, request, id=None):
        print(json.dumps(request), file=self._pipe.stdin)

        if self._verbose >= PsgClient.VerboseLevel.REQUEST:
            print(json.dumps(request))

        self._sent[id or request['id']] = request
        return request

    def receive(self, /, deadline=None):
        while True:
            if deadline is None:
                timeout = None
            else:
                timeout = deadline - time.monotonic()

                if timeout <= 0:
                    raise queue.Empty

            line = self._queue.get(timeout=timeout).rstrip()

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

class SynthPsgClient(PsgClient):
    def __init__(self, args):
        self._request_generator = RequestGenerator()
        super().__init__(args)

    def send(self, method, **params):
        return super().send(*self._request_generator(method, **params))

    def receive(self, /, deadline=None, errors_only=False):
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
        request = self._sent[data.get('id')]
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

        def _any(actual, expected, modify):
            if isinstance(actual, dict):
                if isinstance(expected, dict):
                    return _dict(actual, expected, modify)
            elif isinstance(actual, list):
                if isinstance(expected, list):
                    return _list(actual, expected, modify)
            else:
                return actual == expected

            return False

        def _list(actual, expected, modify, deadline=None):
            i = 0
            success = True
            matching_any = False

            for e in expected:
                if deadline is not None and deadline <= time.monotonic():
                    raise queue.Empty

                if e is None:
                    matching_any = True
                    continue

                for k, a in enumerate(itertools.islice(actual, i, None)):
                    if _any(a, e, modify=False):
                        # Modify only after it fully matches
                        if modify:
                            _any(a, e, modify=True)

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
                return _any(actual[i], expected, modify)

            if modify:
                actual[i] = None

            return True

        _list(self.data, expected.data, modify=True, deadline=deadline)

    def diff(self, other):
        """Produce a unified diff between two responses."""

        def _to_lines(iterable):
            """Transform a response into JSON and split it into list of strings."""
            return list(itertools.chain.from_iterable((json.dumps(value, indent=2) + '\n').splitlines(keepends=True) for value in iterable))

        return list(unified_diff(_to_lines(self), _to_lines(other), fromfile='Expected', tofile='Actual', n=3))

def testcases_cmd(args):
    def enabled(name):
        return name != 'timeout' and (not args.testcase or name in args.testcase) and (not args.no_testcase or name not in args.no_testcase)

    def describe(name, testcase, what):
        desc = testcase['description']
        return '\n\t'.join([f'Testcase{what}:', name] + ([desc] if name != desc else []) + [''])

    total = 0
    succeeded = 0
    check_binary(args)

    with PsgClient(args) as psg_client:
        updated_testcases = {}

        testcases = json.loads(''.join(filter(lambda l: l.lstrip()[:1] not in ['#', ''], args.input_file)))
        overall_deadline = (args.timeout or testcases.get('timeout', 25)) + time.monotonic()

        for (name, testcase) in filter(lambda x: enabled(x[0]), testcases.items()):
            total += 1

            if args.list:
                print(describe(name, testcase, ''))
                continue

            try:
                if overall_deadline <= time.monotonic():
                    raise queue.Empty

                if timeout := testcase.get('timeout'):
                    deadline = min(overall_deadline, timeout + time.monotonic())
                else:
                    deadline = overall_deadline

                actual = []

                for request_batch in testcase['requests']:
                    for i, request in enumerate(request_batch):
                        psg_client.send(request)

                    for i in range(i + 1):
                        actual.extend(psg_client.receive(deadline=deadline))

                expected = Response(testcase['response'])
                actual = Response(actual, actual=True)
                actual.apply(expected, deadline=deadline)
            except queue.Empty:
                print(describe(name, testcase, ' timed out'))
                sys.exit(2)

            if args.output_file:
                testcase['response'] = actual.data
                updated_testcases[name] = testcase
            else:
                diff = expected.diff(actual)

                if diff:
                    print(describe(name, testcase, ' failed'), end='')
                    print(*diff, sep='\t')
                else:
                    succeeded += 1

                    if args.report:
                        print(describe(name, testcase, ' succeeded'))

    if args.output_file:
        with open(args.output_file, 'w') as of:
            json.dump(updated_testcases, of, indent=4)
            of.write('\n')
    elif args.list:
        print(f'{total} testcases total')
    else:
        if args.report:
            print(f'{succeeded} of {total} testcases succeeded')

        if succeeded != total:
            sys.exit(1)

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

        if args.TYPE in ('biodata', 'resolve'):
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
    show_requests = ['biodata', 'blob', 'chunk', 'ipg_resolve', 'named_annot', 'resolve']
    hide_keys = ['conffile', 'debug-printout', 'input-file', 'io-threads', 'logfile', 'max-streams', 'requests-per-io', 'worker-threads', 'rate']
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

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(title='available commands', metavar='COMMAND', required=True, dest='command')

    parser_syntest = subparsers.add_parser('syntest', help='Perform synthetic psg_client tests', description='Perform synthetic psg_client tests', aliases=['test'])
    parser_syntest.set_defaults(func=syntest_cmd)
    parser_syntest.add_argument('-service', help='PSG service/server to use')
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
    parser_testcases.add_argument('-binary', help='psg_client binary to run (default: tries ./psg_client, then $PATH/psg_client)')
    parser_testcases.add_argument('-input-file', help='JSON file with testcases" (default: ./testcases.json)', metavar='FILE', type=argparse.FileType(), default='./testcases.json')
    group1_testcases.add_argument('-output-file', help='Output updated testcases file (e.g. if they need updating)', metavar='FILE')
    group2_testcases.add_argument('-testcase', help='Run only specified testcase(s) (multiple are allowed)', action='append')
    group2_testcases.add_argument('-no-testcase', help='Run all but specified testcase(s) (multiple are allowed)', action='append')
    group1_testcases.add_argument('-report', help='Report testcase results', action='store_true')
    group1_testcases.add_argument('-list', help='List available testcases', action='store_true')
    parser_testcases.add_argument('-data-limit', help='Show a data preview for any data larger the limit (if set)', default='1KB')
    parser_testcases.add_argument('-preview-size', help='How much of data to show as the preview', default='16')
    parser_testcases.add_argument('-timeout', help='Overall timeout (overrides overall timeout from JSON file)', type=int)
    parser_testcases.add_argument('-verbose', '-v', help='Verbose output (multiple are allowed)', action='count', default=0)
    parser_testcases.add_argument('-no-one-server-opt', help=argparse.SUPPRESS, dest='one_server', action='store_false')
    parser_testcases.add_argument('-no-testing-opt', help=argparse.SUPPRESS, dest='testing', action='store_false')

    parser_generate = subparsers.add_parser('generate', help='Generate JSON-RPC requests for psg_client', description='Generate JSON-RPC requests for psg_client')
    parser_generate.set_defaults(func=generate_cmd)
    parser_generate.add_argument('-binary', help='psg_client binary to run (default: tries ./psg_client, then $PATH/psg_client)')
    parser_generate.add_argument('-params', help='Params to add to requests (e.g. \'{"include_info": ["canonical-id", "gi"]}\')')
    parser_generate.add_argument('-verbose', '-v', help='Verbose output (multiple are allowed)', action='count', default=0)
    parser_generate.add_argument('INPUT_FILE', help='CSV file with bio IDs "BioID[,Type]", named annotations "BioID,NamedAnnotID[,NamedAnnotID]... or IPG IDs "Protein,[IPG][,Nucleotide]"', type=argparse.FileType())
    parser_generate.add_argument('TYPE', help='Type of requests (resolve, biodata, blob, named_annot, chunk or ipg_resolve)', metavar='TYPE', choices=['resolve', 'biodata', 'blob', 'named_annot', 'chunk', 'ipg_resolve'])
    parser_generate.add_argument('NUMBER', help='Max number of requests', type=int)

    parser_cgi_help = subparsers.add_parser('cgi_help', help='Generate JSON help for pubseq_gateway.cgi (by parsing psg_client help)', description='Generate JSON help for pubseq_gateway.cgi (by parsing psg_client help)')
    parser_cgi_help.set_defaults(func=cgi_help_cmd)
    parser_cgi_help.add_argument('-binary', help='psg_client binary to run (default: tries ./psg_client, then $PATH/psg_client)')
    parser_cgi_help.add_argument('-output-file', help='Output file (default: %(default)s)', metavar='FILE', default='-', type=argparse.FileType('w'))

    args = parser.parse_args()
    args.func(args)
