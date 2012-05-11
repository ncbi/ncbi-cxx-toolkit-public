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
        self.readbuf = ""
        self.batch_size = 0
        self.batch = []
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
            err_parts = parts[1].split(':')
            if err_parts[0] == "QUEUE_NOT_FOUND" or err_parts[0] == "eUnknownQueue":
                print "Unknown queue: %s" % self.queue
                raise NSException(-2)
            elif err_parts[0] == "eProtocolSyntaxError":
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

    def _submit_batch(self, force=0):
        if not force and len(self.batch) < self.batch_size:
            return 0, None
        self.socket.send("BTCH %d\n" % len(self.batch))
#        print "BTCH %d" % len(self.batch),
#        data_len = 0
        for rec in self.batch:
#            data_len += len(" ".join(rec))
            self.socket.send(" ".join(rec)+"\n")
        self.socket.send("ENDB\n")
#        print "data_len %d" % data_len
        self.batch = []
        parts = self.read_reply()
        if parts[0] != "OK":
            return 1, parts
        return 0, None

    def submit(self, input='', affinity=None, tags=None, mask=0):
        data = ['"'+input+'"']
        if affinity:
            data.append('aff="%s"' % affinity)
        if self.batch_size:
            self.batch.append(data)
            return self._submit_batch()
        self.send_cmd("SUBMIT",  *data)
        parts = self.read_reply()
        if parts[0] != "OK" or len(parts) < 2:
            return 1, parts
        job = Job(parts[1], NSJS_PENDING, input=input)
        return 0, job

    def begin_batch(self, size):
        self.batch = []
        self.batch_size = size
        self.socket.send("BSUB\n")

    def end_batch(self):
        res = self._submit_batch(1)
        self.batch_size = 0
        self.socket.send("ENDS\n")
        return res

    re_get = re.compile("([^ ]+) \"([^\"]*)\" \"([^\"]*)\" \"\" (\d+)")
    def get(self):
        self.send_cmd("GET")
        parts = self.read_reply()
        if parts[0] != "OK":
            return 1, parts
        if len(parts) < 2 or not parts[1].strip():
            return -1, parts
        mo = self.re_get.match(parts[1])
        if not mo: return 2, parts
        jid, input, affinity, job_mask = mo.groups()
        job = Job(jid, NSJS_RUNNING, input=input, affinity=affinity, mask=job_mask)
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
