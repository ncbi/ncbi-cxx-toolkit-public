#!/usr/bin/env python3

import argparse
import ast
import csv
import enum
import itertools
import json
import queue
import random
import shutil
import subprocess
import sys
import threading
import xml.etree.ElementTree as ET

class RequestGenerator:
    def __init__(self):
        self._request_no = 0

    def __call__(self, method, **params):
        self._request_no += 1
        return json.dumps({ 'jsonrpc': '2.0', 'method': method, 'params': params, 'id': f'{method}_{self._request_no}' })

class PsgClient:
    VerboseLevel = enum.IntEnum('VerboseLevel', 'REQUEST RESPONSE DEBUG')

    def __init__(self, binary, /, timeout=10, verbose=0):
        self._cmd = [binary, 'interactive', '-server-mode']
        self._timeout = timeout
        self._verbose = verbose
        self._request_generator = RequestGenerator()

        if self._verbose >= PsgClient.VerboseLevel.DEBUG:
            self._cmd += ['-debug-printout', 'some']

    def __enter__(self):
        self._pipe = subprocess.Popen(self._cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, bufsize=1, text=True)
        self._queue = queue.Queue()
        self._thread = threading.Thread(target=self._run)
        self._thread.start()
        return self

    def _run(self):
        while True:
            try:
                line = self._pipe.stdout.readline()
            except ValueError:
                break
            else:
                self._queue.put(line)

    def __exit__(self, exc_type, exc_value, traceback):
        self._pipe.stdin.close()
        self._pipe.stdout.close()
        self._pipe.wait()
        self._thread.join()

    def send(self, method, **params):
        request = self._request_generator(method, **params)
        print(request, file=self._pipe.stdin)

        if self._verbose >= PsgClient.VerboseLevel.REQUEST:
            print(request)

        return request

    def receive(self):
        while True:
            line = self._queue.get(timeout=self._timeout).rstrip()

            if self._verbose >= PsgClient.VerboseLevel.RESPONSE:
                print(line)

            data = json.loads(line)
            result = data.get('result', None)

            # A JSON-RPC error?
            if result is None:
                yield data if 'error' not in data else { k: v for k, v in data.items() if k == 'error' }
                return

            # An item
            if 'reply' in result:
                yield result
                continue

            # A reply, yield only non-successful ones
            if result['status'] != 'Success':
                yield result
            return

def powerset(iterable):
    s = list(iterable)
    return itertools.chain.from_iterable(itertools.combinations(s, r) for r in range(len(s) + 1))

def test_all(psg_client, bio_ids, blob_ids, named_annots, chunk_ids):
    blob_ids_only = list(blob_id[0] for blob_id in blob_ids['blob_id'])
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
            'exclude_blobs': [None] + list(random.choices(blob_ids_only, k=i) for i in range(4)),
            'acc_substitution': [
                None,
                'default',
                'limited',
                'never'
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

    bio_ids = {'bio_id': list(bio_ids)}

    # Used user_args to increase number of some requests
    requests = {
            'resolve':      [bio_ids,       ['include_info', 'acc_substitution']],
            'biodata':      [bio_ids,       ['include_data', 'exclude_blobs', 'acc_substitution', 'resend_timeout']],
            'blob':         [blob_ids,      ['include_data', 'user_args']],
            'named_annot':  [named_annots,  ['include_data', 'acc_substitution']],
            'chunk':        [chunk_ids,     ['user_args',    'user_args']]
        }

    rv = True
    summary = {}

    for method, (ids, param_names) in requests.items():
        n = 0
        for combination in (this_plus_random(param, value) for param in param_names for value in param_values[param]):
            try:
                random_ids={k: random.choice(v) for k, v in ids.items()}
            except IndexError:
                print(f"Error: 'No IDs for the \"{method}\" requests, skipping'", file=sys.stderr)
                rv = False
                break
            n += 1
            request = psg_client.send(method, **random_ids, **{p: v for p, v in combination if v is not None})
            for reply in psg_client.receive():
                status = reply.get('status', None)
                reply_type = reply.get('reply', None)

                if status is not None:
                    prefix = [reply_type or 'Reply', status]
                    messages = reply['errors']
                    if status not in ('NotFound', 'Forbidden'):
                        rv = False
                elif reply_type is None:
                    error = reply.get('error', {})
                    code = error.get('code', None)
                    prefix = ['Error', code] if code else ['Unknown reply']
                    messages = [error.get('message', reply)]
                    rv = False
                else:
                    continue

                print(*prefix, end=": '", file=sys.stderr)
                print(*messages, sep="', '", end=f"' for request '{request}'\n", file=sys.stderr)
        summary[method] = n

    for method, n in summary.items():
        print(method, n, sep=': ')

    return rv

def get_ids(psg_client, bio_ids, /, max_blob_ids=1000, max_chunk_ids=1000):
    """Get corresponding blob and chunk IDs."""
    blob_ids = set()
    chunk_ids = set()

    for bio_id in bio_ids:
        psg_client.send('biodata', bio_id=bio_id, include_data='no-tse')
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

    return {'blob_id': list(blob_ids)}, {'chunk_id': list(chunk_ids)}


def check_binary(args):
    if shutil.which(args.binary) is None:
        found_binary = shutil.which("psg_client")
        if found_binary is None:
            sys.exit(f'"{args.binary}" does not exist or is not executable')
        else:
            args.binary = found_binary

def read_bio_ids(input_file):
    return ([bio_id] + bio_type for (bio_id, *bio_type) in csv.reader(input_file))

def read_named_annots(input_file):
    return ([bio_id, na_ids] for (bio_id, *na_ids) in csv.reader(input_file))

def prepare_named_annots(named_annots):
    # Powerset of named annotations (excluding empty set)
    named_annots = [([bio_id], subset) for (bio_id, na_ids) in named_annots for subset in powerset(na_ids) if subset]

    # Change into request params for PsgClient
    return {k: [row[i] for row in named_annots] for i, k in enumerate(('bio_id', 'named_annots'))}

def test_cmd(args):
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
                ['NC_000024', ['NA000000067.16', 'NA000134068.1']],
                ['NW_017890465', ['NA000122202.1', 'NA000122203.1']],
                ['NW_024096525', ['NA000288180.1']],
                ['NW_019824422', ['NA000150051.1']],
            ]

    named_annots = prepare_named_annots(named_annots)

    with PsgClient(args.binary, verbose=args.verbose) as psg_client:
        blob_ids, chunk_ids = get_ids(psg_client, bio_ids)
        result = test_all(psg_client, bio_ids, blob_ids, named_annots, chunk_ids)
        sys.exit(0 if result else -1)

def generate_cmd(args):
    request_generator = RequestGenerator()

    if args.TYPE == 'named_annot':
        ids = prepare_named_annots(read_named_annots(args.INPUT_FILE))
    else:
        bio_ids = read_bio_ids(args.INPUT_FILE)

        if args.TYPE in ('biodata', 'resolve'):
            ids = {'bio_id': bio_ids}
        elif args.TYPE in ('blob', 'chunk'):
            check_binary(args)

            max_blob_ids = args.NUMBER if args.TYPE == 'blob' else 0
            max_chunk_ids = args.NUMBER if args.TYPE != 'blob' else 0

            with PsgClient(args.binary) as psg_client:
                blob_ids, chunk_ids = get_ids(psg_client, bio_ids, max_blob_ids=max_blob_ids, max_chunk_ids=max_chunk_ids)

            ids = blob_ids if args.TYPE == 'blob' else chunk_ids

    ids = {k: itertools.cycle(v) for k, v in ids.items()}
    params = {} if args.params is None else ast.literal_eval(args.params)

    try:
        for i in range(args.NUMBER):
            print(request_generator(args.TYPE, **{k: next(v) for k, v in ids.items()}, **params))
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
    show_requests = ['biodata', 'blob', 'chunk', 'named_annot', 'resolve']
    hide_keys = ['conffile', 'debug-printout', 'id-file', 'io-threads', 'logfile', 'max-streams', 'requests-per-io', 'worker-threads']
    hide_flags = ['dryrun', 'latency', 'server-mode']
    tse_flags = ['no-tse', 'slim-tse', 'smart-tse', 'whole-tse', 'orig-tse']

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
            exclude = {name:list(data.keys()) for name, data in exclude.items()}
            requests.add(req_name, req_desc, require, exclude)

            for pos in arg.iterfind('ns:positional', ns):
                pos_name = pos.get('name').lower()
                pos_desc = pos.find('ns:description', ns).text
                params.add(pos_name, 'mandatory', pos_desc, req_name)
                requests.param(req_name, pos_name, pos_desc)

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

            for flag in arg.iterfind('ns:flag', ns):
                flag_name = flag.get('name')

                if flag_name in hide_flags:
                    continue

                flag_desc = flag.find('ns:description', ns).text
                params.add(flag_name, 'flags', flag_desc, req_name)
                requests.param(req_name, flag_name, flag_desc)

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

                # Add TSE flags together, in particular order
                if param_name in tse_flags:
                    if data.get(param_name) is None:
                        data.update({name: '' for name in tse_flags})

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

    result_reqs['resolve'] = {'method': {'GET': get_resolve, 'POST': post_resolve}, 'Request-specific Parameters': resolve_params}
    result = {'request': result_reqs, 'Common Parameters': common_params}
    json.dump(result, args.output_file, indent=2)

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(title='available commands', metavar='COMMAND', required=True, dest='command')

    parser_test = subparsers.add_parser('test', help='Perform psg_client tests', description='Perform psg_client tests')
    parser_test.set_defaults(func=test_cmd)
    parser_test.add_argument('-binary', help='psg_client binary to run (default: tries ./psg_client, then $PATH/psg_client)', default='./psg_client')
    parser_test.add_argument('-bio-file', help='CSV file with bio IDs "BioID[,Type]" (default: some hard-coded IDs)', type=argparse.FileType())
    parser_test.add_argument('-na-file', help='CSV file with named annotations "BioID,NamedAnnotID[,NamedAnnotID]..." (default: some hard-coded IDs)', type=argparse.FileType())
    parser_test.add_argument('-verbose', '-v', help='Verbose output (multiple are allowed)', action='count', default=0)

    parser_generate = subparsers.add_parser('generate', help='Generate JSON-RPC requests for psg_client', description='Generate JSON-RPC requests for psg_client')
    parser_generate.set_defaults(func=generate_cmd)
    parser_generate.add_argument('-binary', help='psg_client binary to run (default: tries ./psg_client, then $PATH/psg_client)', default='./psg_client')
    parser_generate.add_argument('-params', help='Params to add to requests (e.g. \'{"include_info": ["canonical-id", "gi"]}\')')
    parser_generate.add_argument('INPUT_FILE', help='CSV file with bio IDs "BioID[,Type]" or named annotations "BioID,NamedAnnotID[,NamedAnnotID]..."', type=argparse.FileType())
    parser_generate.add_argument('TYPE', help='Type of requests (resolve, biodata, blob, named_annot or chunk)', metavar='TYPE', choices=['resolve', 'biodata', 'blob', 'named_annot', 'chunk'])
    parser_generate.add_argument('NUMBER', help='Max number of requests', type=int)

    parser_cgi_help = subparsers.add_parser('cgi_help', help='Generate JSON help for pubseq_gateway.cgi (by parsing psg_client help)', description='Generate JSON help for pubseq_gateway.cgi (by parsing psg_client help)')
    parser_cgi_help.set_defaults(func=cgi_help_cmd)
    parser_cgi_help.add_argument('-binary', help='psg_client binary to run (default: tries ./psg_client, then $PATH/psg_client)', default='./psg_client')
    parser_cgi_help.add_argument('-output-file', help='Output file (default: %(default)s)', metavar='FILE', default='-', type=argparse.FileType('w'))

    args = parser.parse_args()
    args.func(args)
