#! /bin/sh
# $Id$

exit_code=0
client_log=test_ncbi_socket_client.log
server_log=test_ncbi_socket_server.log

rm -f $client_log $server_log

CONN_DEBUG_PRINTOUT=SOME;  export CONN_DEBUG_PRINTOUT

port="555`expr $$ % 100`"

test_ncbi_socket $port >>$server_log 2>&1 &
spid=$!
trap 'kill -9 $spid' 0 1 2 15

sleep 1
$CHECK_EXEC test_ncbi_socket localhost $port >>$client_log  ||  exit_code=1

kill $spid  ||  exit_code=2
( kill -9 $spid ) >/dev/null 2>&1

if [ ${exit_code:-0} != 0 ]; then
  if [ -s $client_log ]; then
    echo "=== $client_log ==="
    cat $client_log
  fi
  if [ -s $server_log ]; then
    echo "=== $server_log ==="
    cat $server_log
  fi
fi

exit $exit_code
