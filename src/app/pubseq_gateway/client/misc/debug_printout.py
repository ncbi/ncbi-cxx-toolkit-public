#!/usr/bin/env python3

import fileinput
import urllib.parse
import warnings

def parse_psg_args(message):
    parsed = urllib.parse.parse_qs(message.split('\\n', maxsplit=1)[0])
    return {x: y[0] for x, y in parsed.items()}

class Processor:
    def __init__(self):
        self._data = {}

    def stop(self):
        if self._data:
            warnings.warn('_data is not empty')

class SortRepliesByRequests(Processor):
    def message(self, request_id, message):
        self._data.setdefault(request_id, []).append(message)

    def closed(self, request_id, message):
        for stored in self._data.pop(request_id, []):
            print(f'{request_id}:', stored)
        print(f'{request_id}:', message)

class CheckSentReplies(Processor):
    def __init__(self):
        super().__init__()
        self._first_sent = None

    def message(self, request_id, message):
        data = parse_psg_args(message)
        if data.get('reason') == 'sent':
            self._data.setdefault(request_id, set()).add((data['blob_id'], data['last_modified']))

    def closed(self, request_id, message):
        sent = self._data.get(request_id)
        if sent:
            if self._first_sent is None:
                self._first_sent = sent
            elif sent == self._first_sent:
                print('Matched:', len(sent), "'sent' replies")
            else:
                print(len(sent), len(self._first_sent), sent - self._first_sent)
            del self._data[request_id]

def process(processor):
    for line in fileinput.input():
        split = line.rstrip().split(maxsplit=2)
        request_id = split[1].rstrip(':')
        if split[2] == 'Closed with status NO_ERROR':
            processor.closed(request_id, split[2])
        else:
            processor.message(request_id, split[2])
    processor.stop()

if __name__ == '__main__':
    process(SortRepliesByRequests())
