#! /bin/sh
# $Id$

status=0
port="555`expr $$ % 100`"

./test_threaded_server -port $port &
server_pid=$!
trap 'kill $server_pid' 1 2 15
sleep 1
./test_threaded_client -port $port -requests 34 || status=1
kill $server_pid || status=2
exit $status
