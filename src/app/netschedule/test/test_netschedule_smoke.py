#!/usr/bin/env python

import sys
import ns
        
class TestConnection(ns.Connection):
    def __init__(self, host, port, queue):
        ns.Connection.__init__(self, host, port, queue)
        self.successes = {}
        self.failures = {}

    def check_status_for_jobs(self, jobs):
        for jid, job in jobs.iteritems():
            res, status, job_reply = self.check_status(jid)
            if res:
                print "Error %d in check_status" % res
                self.failures.setdefault("STATUS", []).append(None) # TODO: command may vary
                continue
            if status != job.status:
                print "Status mismatch for job %s: required %d, actual %d" % (jid, job.status, status)

    def test_single_task_life_cycle(self):
        jobs = {}
        self.failures = {}
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
            res, job = self.submit(data, affinity="aff_tok")
            if res:
                print "At jdata length %d" % l,
                print "Error in SUBMIT: %s" % ": ".join(parts[1:])
                self.failures.setdefault("SUBMIT", []).append(data)
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
                self.failures.setdefault("GET", []).append(job)
                continue
            if res == 2:
                print "Can't parse reply:", job
                self.failures.setdefault("GET", []).append(job)
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
            if not cmd in self.failures:
                self.successes[cmd] = 1
        if self.failures: return 1
        return 0

def test(host, port, queue):
    try:
        con = TestConnection(host, port, queue)
        con.login()
    except ns.NSException, e:
        return e.errcode
    if con.params['srv_gen'] < 2:
        print "Can't test server, generation %d is less than required 2" % con.params['srv_gen']
        return -3
    total_res = 0
    res = con.test_single_task_life_cycle()
    if not res: total_res = res
    if total_res:
        print "%d errors" % total_res
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
