#!/bin/bash
# $Header$

# Simulate a broken database server for the 'dbapi_svc_test' Testsuite test per CXX-10698.
# Meant to be called from crontab as a watcher script, but can be run standalone.
# Grepping out 'getpeername failed' messages is done to ignore failed connections
# caused by LBSMD's polling, but to allow genuine errors to propagate.
# See 'dbapi_svc_test.cpp' for the test code.

resp="I am a broken server."
port=1444
bin=/bin
$bin/ps -fu $USER | $bin/grep -F "$resp" | $bin/grep -qv "grep -F"
[[ ${PIPESTATUS[@]} == "0 0 0" ]] || \
    $bin/ncat --listen --send-only --keep-open --max-conns 4 --exec "$bin/echo $resp" $port \
    2> >($bin/grep -F -v 'Ncat: getpeername failed: Transport endpoint is not connected QUITTING.' >&2) &
