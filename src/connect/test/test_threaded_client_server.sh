#! /bin/sh
# $Id$

status=0
port="555`expr $$ % 100`"

./test_threaded_server -port $port -queue 50 &
server_pid=$!
trap 'kill $server_pid' 1 2 15
sleep 1
./test_threaded_client -port $port || status=1
kill $server_pid || status=2
exit $status
