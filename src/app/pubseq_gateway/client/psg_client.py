#!/usr/bin/env python3

import argparse
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

class PsgClient:
    VerboseLevel = enum.IntEnum('VerboseLevel', 'REQUEST RESPONSE DEBUG')

    def __init__(self, binary, /, timeout=10, verbose=0):
        self._cmd = [binary, 'interactive', '-server-mode']
        self._timeout = timeout
        self._verbose = verbose
        self._request_no = 0

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
        self._request_no += 1
        request = json.dumps({ 'jsonrpc': '2.0', 'method': method, 'params': params, 'id': f'{method}_{self._request_no}' })
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

    # Used user_args to increase number of some requests
    requests = {
            'resolve':      [bio_ids,       ['include_info', 'acc_substitution']],
            'biodata':      [bio_ids,       ['include_data', 'acc_substitution', 'resend_timeout']],
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

def get_ids(psg_client, bio_ids, blob_ids, chunk_ids):
    # Get corresponding blob and chunk IDs
    for bio_id in bio_ids['bio_id']:
        psg_client.send('biodata', bio_id=bio_id, include_data='no-tse')
        for reply in psg_client.receive():
            item = reply.get('reply', None)

            if item == 'BlobInfo':
                blob_id = reply.get('id', {}).values()
                if blob_id:
                    blob_ids['blob_id'].append(list(blob_id))

                id2_info = reply.get('id2_info', '')
                if id2_info:
                    chunk = id2_info.split('.')[2]
                    if chunk:
                        for i in range(int(chunk)):
                            chunk_ids['chunk_id'].append([i + 1, id2_info])
                        chunk_ids['chunk_id'].append([999999999, id2_info])

def test_cmd(args):
    if shutil.which(args.binary) is None:
        found_binary = shutil.which("psg_client")
        if found_binary is None:
            sys.exit(f'"{args.binary}" does not exist or is not executable')
        else:
            args.binary = found_binary

    if args.bio_file:
        bio_ids = ([bio_id] + bio_type for (bio_id, *bio_type) in csv.reader(args.bio_file))
    else:
        bio_ids = (
                ['emb|CQD33614.1'], ['emb|CQD24742.1'], ['emb|CQD05473.1'], ['emb|CQD25256.1'], ['emb|CPW37052.1'],
                ['emb|CPW38273.1'], ['emb|CPW43357.1'], ['emb|CPW37084.1'], ['emb|CQD02773.1'], ['emb|CQD03543.1'],
                ['emb|CQD06500.1'], ['emb|CQD06925.1'], ['emb|CRH17606.1'], ['emb|CRH18613.1'], ['emb|CRH28774.1'],
                ['emb|CRH31178.1'], ['emb|CRK75219.1'], ['emb|CRK74868.1'], ['emb|CRK82124.1'], ['emb|CRK85455.1'],
                ['emb|CRL52170.1'], ['emb|CRL52726.1'], ['emb|CRL57344.1'], ['emb|CRL57676.1'], ['emb|CRM10744.1'],
                ['emb|CRM09785.1'], ['emb|CRM17222.1'], ['emb|CRM17266.1'], ['emb|CQD30058.1'], ['emb|CQD27426.1'],
            )

    if args.na_file:
        named_annots = ([bio_id, na_ids] for (bio_id, *na_ids) in csv.reader(args.na_file))
    else:
        named_annots = (
                ['NC_000024', ['NA000000067.16', 'NA000134068.1']],
                ['NW_017890465', ['NA000122202.1', 'NA000122203.1']],
                ['NW_024096525', ['NA000288180.1']],
                ['NW_019824422', ['NA000150051.1']],
            )

    bio_ids = {'bio_id': list(bio_ids)}
    blob_ids = {'blob_id': []}
    chunk_ids = {'chunk_id': []}

    # Powerset of named annotations (excluding empty set)
    named_annots = [([bio_id], subset) for (bio_id, na_ids) in list(named_annots) for subset in powerset(na_ids) if subset]

    # Change into request params for PsgClient
    named_annots = {k: [row[i] for row in list(named_annots)] for i, k in enumerate(('bio_id', 'named_annots'))}

    with PsgClient(args.binary, verbose=args.verbose) as psg_client:
        get_ids(psg_client, bio_ids, blob_ids, chunk_ids)
        result = test_all(psg_client, bio_ids, blob_ids, named_annots, chunk_ids)
        sys.exit(0 if result else -1)

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(title='available commands', metavar='COMMAND', required=True, dest='command')

    parser_test = subparsers.add_parser('test', help='Perform psg_client tests', description='Perform psg_client tests')
    parser_test.set_defaults(func=test_cmd)
    parser_test.add_argument('-binary', help='psg_client binary to run (default: tries ./psg_client, then $PATH/psg_client)', default='./psg_client')
    parser_test.add_argument('-bio-file', help='CSV file with bio IDs "BioID[,Type]" (default: some hard-coded IDs)', type=argparse.FileType())
    parser_test.add_argument('-na-file', help='CSV file with named annotations "BioID,NamedAnnotID[,NamedAnnotID]..." (default: some hard-coded IDs)', type=argparse.FileType())
    parser_test.add_argument('-verbose', '-v', help='Verbose output (multiple are allowed)', action='count', default=0)

    args = parser.parse_args()
    args.func(args)
