#! /bin/sh
# $Id$

status=0
./test_threaded_server -port 12345 -queue 50 &
server_pid=$!
trap 'kill $server_pid' 1 2 15
sleep 1
./test_threaded_client -port 12345 || status=1
kill $server_pid || status=2
exit $status
