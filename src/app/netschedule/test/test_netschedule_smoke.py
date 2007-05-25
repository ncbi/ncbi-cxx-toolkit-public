#!/usr/bin/env python

import sys, socket, re

class NSException(Exception):
    def __init__(self, errcode, msg=""):
        self.errcode = errcode
        self.msg = msg
    def __str__(self):
        return "Error %d: %s" % (self.errcode, self.msg)

NSJS_NOTFOUND = -1
NSJS_PENDING  = 0
NSJS_RUNNING  = 1
NSJS_RETURNED = 2
NSJS_CANCELED = 3
NSJS_FAILED   = 4
NSJS_DONE     = 5
        
class Job:
    def __init__(self, jid, status=NSJS_PENDING, **kw):
        self.jid = jid
        self.status = status
        self.input = ''
        self.output = ''
        self.error = ''
        self.retcode = 0
        self.mask = 0
        self.affinity = None
        self.tags = []
        self.__dict__.update(kw)
        

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

    def drop_queue(self):
        self.send_cmd("DROPQ")
        self.read_reply()
        
    def submit(self, input='', affinity=None, tags=None, mask=0):
        self.send_cmd("SUBMIT",  '"'+input+'"')
        parts = self.read_reply()
        if parts[0] != "OK" or len(parts) < 2:
            return 1, parts
        job = Job(parts[1], NSJS_PENDING, input=input)
        return 0, job
	
    re_get = re.compile("([^ ]+) \"([^\"]*)\" \"\" \"\" (\d+)")
    def get(self):
        self.send_cmd("GET")
        parts = self.read_reply()
        if parts[0] != "OK":
            return 1, parts
        if len(parts) < 2 or not parts[1].strip():
            return -1, parts
        mo = self.re_get.match(parts[1])
        if not mo: return 2, parts
        jid, input, job_mask = mo.groups()
        job = Job(jid, NSJS_RUNNING, input=input, mask=job_mask)
        return 0, job
	
    def put(self, jid, retcode, output):
        pass

    def exchange(self, jid, retcode, output):
        pass
	
    def fail(self, jid, retcode, error):
        pass

    def cancel(self, jid):
        pass
	
	re_status = re.compile("(\d+) (-?\d+) \"([^\"]*)\" \"([^\"]*)\" \"([^\"]*)\"") 
    def check_status(self, jid, fast=1):
        fast = fast and self.params.get('fast_status', 0)
        if fast: self.send_cmd("SST", jid)
        else: self.send_cmd("STATUS", jid)
        parts = self.read_reply()
        if parts[0] != "OK":
            return 1, -1, parts
        status = int(parts[1])
        if status == -1 or fast:
            return 0, status, None
        mo = self.re_status.match(parts[1])
        if not mo: return 2, -1, parts
        status, retcode, output, error, input = mo.groups()
        status = int(status)
        retcode = int(retcode)
        job = Job(jid, status, input=input, output=output, retcode=retcode, error=error)
        return 0, status, job
        

    def check_status_for_jobs(self, jobs):
        for jid, job in jobs.iteritems():
            res, status, job_reply = self.check_status(jid)
            if res:
                print "Error %d in check_status" % res
                failures.setdefault("STATUS", []).append(None) # TODO: command may vary
                continue
            if status != job.status:
                print "Status mismatch for job %s: required %d, actual %d" % (jid, job.status, status)

    def test_single_task_life_cycle(self):
        jobs = {}
        failures = {}
        self.drop_queue()
        # Submit jobs
        for l in [0, 1, 4, 16, 63, 64, 65, 2047, 2048, 2049, 2050, 6505,
	          16*1024-8, 64*1024, 1024*1024]:
            if l > self.params['max_input_size']:
                continue
            try:
                data = file("SUBMIT_"+str(l)).read()
            except:
                data = " " * l
            res, job = self.submit(data)
            if res:
                print "At jdata length %d" % l,
                print "Error in SUBMIT: %s" % ": ".join(parts[1:])
                failures.setdefault("SUBMIT", []).append(data)
                continue
            jobs[job.jid] = job
        # Check their status
        self.check_status_for_jobs(jobs)
        # Get them and put results back
        while 1:
	    res, job = self.get()
            if res < 0:
                break
	    if res == 1:
                print "Error in GET"
                failures.setdefault("GET", []).append(job)
                continue
            if res == 2:
                print "Can't parse reply:", job
                failures.setdefault("GET", []).append(job)
                continue
            subm_job = jobs.get(job.jid)
            if not subm_job:
                print "I did not submit job %s!" % job.jid
                continue
            if subm_job.input != job.input:
                print "At data length %d" % len(subm_job.input),
                print "Error in GET: data mismatch src len: %d, ret len: %d" % (len(subm_job.input), len(job.input))
                print "Tails: src %s, ret %s" % (map(lambda x:hex(ord(x)), subm_job.input[-10:]), map(lambda x:hex(ord(x)), job.input[-10:]))
	    jobs[job.jid] = job
        # Check status
        self.check_status_for_jobs(jobs)
        # Put result/exchange
        # Check status
        # Fail jobs
        # Check status
        # Reschedule jobs
        # Check status
        # Get jobs
        # Return jobs
        # Cancel jobs
        for cmd in ['SUBMIT', 'GET', 'STATUS']:
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
