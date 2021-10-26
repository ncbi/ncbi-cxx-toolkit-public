import csv
from enum import IntEnum, auto
from statistics import mean
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

class TotalTime:
    def __init__(self):
        self._min = None
        self._max = None

    def add(self, epoch):
        e = float(epoch)
        self._min = min(self._min or e, e)
        self._max = max(self._max or e, e)

    def __str__(self):
        return f'{(self._max - self._min) / 1000:7.3f}'

class Success:
    def __init__(self):
        self._total = 0

    def add(self, result):
        if result == 'true':
            self._total += 1

    def __str__(self):
        return f'{self._total}'

rules = {
    'StartToDone':      [[ EventType.START,   EventIndex.FIRST ], [ EventType.DONE,    EventIndex.FIRST ]],
    'SendToClose':      [[ EventType.SEND,    EventIndex.FIRST ], [ EventType.CLOSE,   EventIndex.FIRST ]],

    'StartToSend':      [[ EventType.START,   EventIndex.FIRST ], [ EventType.SEND,    EventIndex.FIRST ]],
    'SendToFirstChunk': [[ EventType.SEND,    EventIndex.FIRST ], [ EventType.RECEIVE, EventIndex.FIRST ]],
    'FirstToLastChunk': [[ EventType.RECEIVE, EventIndex.FIRST ], [ EventType.RECEIVE, EventIndex.LAST  ]],
    'LastChunkToClose': [[ EventType.RECEIVE, EventIndex.LAST  ], [ EventType.CLOSE,   EventIndex.FIRST ]],
    'CloseToDone':      [[ EventType.CLOSE,   EventIndex.FIRST ], [ EventType.DONE,    EventIndex.FIRST ]],

    'StartToSubmit':    [[ EventType.START,   EventIndex.FIRST ], [ EventType.SUBMIT,  EventIndex.FIRST ]],
    'SubmitToReply':    [[ EventType.SUBMIT,  EventIndex.FIRST ], [ EventType.REPLY,   EventIndex.FIRST ]],
    'ReplyToDone':      [[ EventType.REPLY,   EventIndex.FIRST ], [ EventType.DONE,    EventIndex.FIRST ]],
}

reader = csv.reader(sys.stdin, delimiter='\t')
requests = {}
total_time = TotalTime()

for line in reader:
    if len(line) < EventField.REST:
        pass

    epoch = line[EventField.EPOCH]
    data = { EventField.EPOCH: epoch }
    total_time.add(epoch)

    if len(line) > EventField.REST:
        data[EventField.REST] = dict([ s.split('=') for s in line[EventField.REST:] ])

    request_id = line[EventField.REQUEST_ID]
    event_type = EventType(int(line[EventField.TYPE]))
    requests.setdefault(request_id, {}).setdefault(event_type, []).append(data)

print('\t'.join([ 'Request', *rules.keys(), 'Success' ]))
rules_data = {}
success = Success()

for request_id, request_data in requests.items():
    row = {}
    for rule_name, rule_params in rules.items():
        ((start_type, start_index), (end_type, end_index)) = rule_params
        start = request_data[start_type][start_index.value][EventField.EPOCH]
        end = request_data[end_type][end_index.value][EventField.EPOCH]
        time = float(end) - float(start)
        row[rule_name] = time
        rules_data.setdefault(rule_name, []).append(time)

    converted_row_values = [ f'{v:7.3f}' for v in row.values() ]
    request_result = request_data[EventType.DONE][EventIndex.FIRST][EventField.REST]['success']
    success.add(request_result)
    print('\t'.join([ request_id, *converted_row_values, request_result ]))

print()
summary = {}
get_percentile = lambda d, p: rule_data[int(p * len(d)) - 1]

for rule_name, rule_data in rules_data.items():
    rule_data.sort()
    summary.setdefault('Min ', []).append(rule_data[0])
    summary.setdefault('25th', []).append(get_percentile(rule_data, 0.25))
    summary.setdefault('Median', []).append(get_percentile(rule_data, 0.50))
    summary.setdefault('75th', []).append(get_percentile(rule_data, 0.75))
    summary.setdefault('90th', []).append(get_percentile(rule_data, 0.90))
    summary.setdefault('95th', []).append(get_percentile(rule_data, 0.95))
    summary.setdefault('Max ', []).append(rule_data[-1])
    summary.setdefault('Average', []).append(mean(rule_data))

for row_name, row_data in summary.items():
    values = [ f'{v:7.3f}' for v in row_data ]
    print('\t'.join([ row_name, *values ]))

print(f'Success\t{success}/{len(requests)}\trequests')
print(f'Overall\t{total_time}\tseconds')
