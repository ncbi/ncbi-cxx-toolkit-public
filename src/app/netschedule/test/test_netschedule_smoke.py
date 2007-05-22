#!/usr/bin/env python

import sys, socket, re

class NSException(Exception):
    def __init__(self, errcode, msg=""):
        self.errcode = errcode
        self.msg = msg
    def __str__(self):
        return "Error %d: %s" % (self.errcode, self.msg)

class Connection:
    def __init__(self, host, port, queue):
        self.socket = None
        self.host = host
        self.port = port
        self.queue = queue
        self.params = {}
        self.successes = {}
        self.failures = {}
        self.readbuf = ""
        self.connect()

    def connect(self):
        s = self.socket = socket.socket()
        try: s.connect((self.host, self.port))
        except:
            print "Can't connect to %s:%d" % (self.host, self.port)
            raise NSException(-1)

    def login(self):
        s = self.socket
        s.send("netschedule_admin\n%s\n" % self.queue)
        # Try GETP
        s.send("GETP\n")
        srv_gen = 0
        parts = self.read_reply()
        self.params['max_input_size'] = 2048
        self.params['max_output_size'] = 2048
        if parts[0] == "ERR":
            if parts[1] == "QUEUE_NOT_FOUND" or parts[1] == "eUnknownQueue":
                print "Unknown queue: %s" % self.queue
                raise NSException(-2)
            elif parts[1] == "eProtocolSyntaxError":
                srv_gen = 1
        elif not parts[1]:
            srv_gen = 0
        else:
            srv_gen = 2
            str_params = parts[1].split(';')
            if len(str_params) > 1 and not str_params[-1]:
                str_params = str_params[:-1]
            for param in str_params:
                key, val = param.split('=')
                try: val = int(val)
                except: pass
                self.params[key] = val
        self.params['srv_gen'] = srv_gen
        
    def send_cmd(self, *params):
        self.socket.send(" ".join(params)+"\n")


    re_reply_delimiter = re.compile('\r\n|\n|\r')
    def read_reply(self):
        while 1:
            reply = self.socket.recv(4096)
            parts = self.re_reply_delimiter.split(reply, 1)
            if len(parts) > 1:
                reply = self.readbuf+parts[0]
                self.readbuf = parts[1]
                break
            else:
                self.readbuf += reply
        parts = reply.split(':', 1)
        return parts
    
    re_get = re.compile("([^ ]+) \"([^\"]*)\" \"\" \"\" (\d+)")
    def test_single_task_life_cycle(self):
        submitted = {}
        executing = []
        failures = {}
        s = self.socket
        self.send_cmd("DROPQ")
        self.read_reply()
        for l in [0, 1, 4, 16, 63, 64, 65, 2047, 2048, 2049, 2050, 6505, 16*1024-8, 64*1024, 1024*1024-4]:
            if l > self.params['max_input_size']:
                continue
            if l == 6505:
                data = file("sb_job_out_before_ps.txt").read()
            else:
                data = " " * l
            self.send_cmd("SUBMIT",  '"'+data+'"')
            parts = self.read_reply()
            if parts[0] == "ERR" or len(parts) < 2:
                print "At data length %d" % l,
                print "Error in SUBMIT: %s" % ": ".join(parts[1:])
                failures.setdefault("SUBMIT", []).append(data)
                continue
            submitted[parts[1]] = data
        while 1:
            self.send_cmd("GET")
            parts = self.read_reply()
            if parts[0] == "ERR":
                print "At data length %d" % l,
                print "Error in GET: %s" % ": ".join(parts[1:])
                failures.setdefault("GET", []).append(data)
                continue
            if len(parts) < 2 or not parts[1].strip():
                break
            mo = self.re_get.match(parts[1])
            if not mo:
                print "Can't parse reply:", parts[1]
                continue
            jid, data, job_mask = mo.groups()
            if submitted[jid] != data:
                print "At data length %d" % len(submitted[jid]),
                print "Error in GET: data mismatch"
            executing.append(parts[1])
        for cmd in ['SUBMIT', 'GET']:
            if not cmd in failures:
                self.successes[cmd] = 1
        if failures: return 1
        return 0

def test(host, port, queue):
    try:
        con = Connection(host, port, queue)
        con.login()
    except NSException, e:
        return e.errcode
    if con.params['srv_gen'] < 2:
        print "Can't test server, generation %d is less than required 2" % con.params['srv_gen']
        return -3
    total_res = 0
    res = con.test_single_task_life_cycle()
    if not res: total_res = res
    return total_res


def main(args):
    if len(args) < 2:
        print "Usage: test_netschedule_smoke.py host port [queue]"
        return 1
    host = args[0]
    port = int(args[1])
    queue = "test"
    if len(args) > 2: queue = args[2]
    # Error codes: 0 - success, >0 - test errors, <0 - unable to test
    # -1 - can't connect, -2 - unknown queue, -3 - old generation
    return test(host, port, queue)

if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
