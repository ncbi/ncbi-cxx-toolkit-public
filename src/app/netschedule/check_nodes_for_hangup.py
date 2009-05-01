#!/usr/bin/env python
import sys, socket, re
from optparse import OptionParser

re_reply_delimiter = re.compile('\r\n|\n|\r|\0')
re_node_info = re.compile(r"(?:\w+ prog='.+')? @ ((?:[-0-9a-zA-Z]+\.)*[-0-9a-zA-Z]+?)[^\w]+UDP:(\d+) \d+/\d+/\d+ \d+:\d+:\d+ \(\d+\) (?:node_id=([\w]+) )?jobs= ?(.*)")
debug = False

class Connection:
    def __init__(self, host, port, queue=""):
        self.read_buf = ""
        self.sock = None
        self.host = host
        self.port = port
        self.queue = queue

    def connect(self, timeout=None):
        self.sock = sock = socket.socket()
        sock.settimeout(timeout)
        sock.connect((self.host, self.port))

    def login(self):
        self.sock.send("netschedule_admin\n%s\n" % self.queue)

    def send_cmd(self, cmd):
        self.sock.send(cmd+"\n")

    def read_reply(self):
        while 1:
            parts = re_reply_delimiter.split(self.read_buf, 1)
            if len(parts) > 1:
                reply = parts[0]
                self.read_buf = parts[1]
                break
            else:
                r = self.sock.recv(4096)
                if not r:
                    if self.read_buf:
                        return "OK", self.read_buf
                    return "", None
                self.read_buf += r
        if reply.startswith("OK:") or reply.startswith("ERR:"):
            return reply.split(':', 1)
        else:
            return "OK", reply

    def read_reply_multi(self):
        rr = []
        res = self.read_reply()
        while res[0] != "ERR" and (res[0] != "OK" or res[1] != "END"):
            if res[1]:
                rr.append(res[1])
            res = self.read_reply()
        if res[0] == "ERR":
            return res
        return "OK", rr

    def stat_wnode(self):
        self.send_cmd("STAT WNODE")
        nodes = []
        res = self.read_reply()
        while res[0] != "ERR" and (res[0] != "OK" or res[1] != "END"):
            if res[1]:
                mo = re_node_info.match(res[1])
                if not mo:
                    if debug: print >>sys.stderr, "No match for:", res[1]
                    return None
                host, port_s, node_id, jobs_s = mo.groups()
                port = int(port_s)
                jobs = jobs_s.split()
                nodes.append((host, port, node_id, jobs))
            res = self.read_reply()
        if res[0] == "ERR":
            return None
        return nodes
    def reschedule(self, job):
        self.send_cmd("FRES %s" % job)
        return self.read_reply()
    def s_status(self, job):
        self.send_cmd("SST %s" % job)
        return self.read_reply()
    def query(self, cond, fields):
        self.send_cmd("QERY \"%s\" SLCT \"%s\"" % (cond, ",".join(fields)))
        res = self.read_reply_multi()
        if res[0] != "OK":
            if debug: print >>sys.stderr, "Bad query: ", res[1]
            return None
        return map(lambda x: x.split('\t'), res[1])
    def ver(self):
        self.send_cmd("VERSION")
        return self.read_reply()
    def clear_node(self, node_id):
        self.send_cmd("CLRN \"%s\"" % node_id)
        return self.read_reply()


def check_alive(nodes, timeout):
    alive = []
    dead = []
    for node in nodes:
        host, port, node_id, jobs = node
        try:
            c = Connection(host, port)
            c.connect(timeout)
            c.login()
            res = c.ver()
            if debug:
                print >>sys.stderr, "Node %s @ %s:%d is alive" % (node_id, host, port)
            alive.append(node)
        except socket.timeout:
            if debug:
                print >>sys.stderr, "Node %s @ %s:%d is dead, timeout" % (node_id, host, port)
            dead.append(node)
        except socket.error:
            if debug:
                print >>sys.stderr, "Node %s @ %s:%d is dead, no connection" % (node_id, host, port)
            dead.append(node)
    return alive, dead

def main():
    global debug
    parser = OptionParser(usage="usage: %prog [options] host port queue")
    parser.add_option("-t", "--timeout", dest="timeout", type="int", default=5,
                      help="Worker Node timeout")
    parser.add_option("-n", "--dry-run", dest="dry_run", action="store_true", default=False,
                      help="Do not reschedule jobs and clear nodes, just show dead nodes")
    parser.add_option("-d", "--detail", dest="detail", action="store_true", default=False,
                      help="Detailed cluster status report")
    parser.add_option("-g", "--debug", dest="debug", action="store_true", default=False,
                      help="Print out program reasoning")
    options, args = parser.parse_args()
    if len(args) < 3:
        parser.error("Not enough arguments")
    debug = options.debug

    host, port, queue = args[:3]
    port = int(port)
    c = Connection(host, port, queue)
    try: c.connect(30)
    except:
        print >>sys.stderr, "Can't connect to NetSchedule at %s:%s" % (host, port)
        return 3
    c.login()
    nodes = c.stat_wnode()
    print "nodes=%d" % len(nodes)
    alive, dead = check_alive(nodes, options.timeout)
    print "alive=%d" % len(alive)
    print "dead=%d" % len(dead)
    jobs_hung = 0
    jobs_rescheduled = 0
    for node in dead:
        host, port, node_id, jobs = node
        jobs_hung += len(jobs)
        if not port:
            if debug: print >>sys.stderr, "Node without port, can't do anything"
            continue
        if options.detail:
            print "host=%s&port=%d&node=%s&jobs=%s" % (host, port, node_id, ",".join(map(str, jobs)))
        if options.dry_run: continue
        if node_id:
            if debug: print >>sys.stderr, "Node with id, CLRN"
            res = c.clear_node(node_id)
            if res[0] == "OK": jobs_rescheduled += len(jobs)
        else:
            if debug: print >>sys.stderr, "Node without id, FRES jobs one by one"
            for job in jobs:
                res = c.s_status(job)
                if res[0] == "OK" and res[1] == "1":
                    res = c.query("id="+job, ["node_id"])
                    if res and res[0][0] == node_id:
                        res = c.reschedule(job)
                        if res[0] == "OK": jobs_rescheduled += 1
    print "jobs_hung=%d" % jobs_hung
    if not options.dry_run:
        print "rescheduled=%d" % jobs_rescheduled
    return 0

if __name__ == "__main__":
    sys.exit(main())
