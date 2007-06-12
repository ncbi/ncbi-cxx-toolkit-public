#!/usr/bin/env python

import sys, time
import ns

def test(host, port, queue, fname, size):
    con = ns.Connection(host, port, queue)
    con.login()
    t_begin = time.time()
    batch_size = 5000
    con.begin_batch(batch_size)
    i = 0
    for line in file(fname):
        line = line.strip()
        res, info = con.submit(line)
        if res:
            print "Error submitting record %d: %s" % (i, info)
            break
        i += 1
        if i % batch_size == 0:
            print "Submitted %d jobs, elapsed %d sec" % (i, time.time() - t_begin)
    con.end_batch()
        
def main(args):
    if len(args) < 4:
        print "Usage: test_netschedule_bsub.py host port queue file [size]"
        return 1
    host = args[0]
    port = int(args[1])
    queue = args[2]
    fname = args[3]
    size = 5000
    if len(args) > 4: size = int(args[4])
    return test(host, port, queue, fname, size)

if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
