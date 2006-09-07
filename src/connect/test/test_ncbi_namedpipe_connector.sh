#! /bin/sh
# $Id$

exit_code=0
client_log=test_namedpipe_con_client.log
server_log=test_namedpipe_con_server.log

rm -f $client_log $server_log

CONN_DEBUG_PRINTOUT=ALL; export CONN_DEBUG_PRINTOUT

test_ncbi_namedpipe_connector server >>$server_log 2>&1 &

server_pid=$!
trap 'kill -9 $server_pid' 1 2 15

sleep 10
$CHECK_EXEC test_ncbi_namedpipe_connector client >>$client_log 2>&1  ||  exit_code=1

kill $server_pid  ||  exit_code=2
( kill -9 $server_pid ) >/dev/null 2>&1
if [ $exit_code != 0 ]; then
  if [ -s $client_log ]; then
    echo "=== $client_log ==="
    cat $client_log
  fi
  if [ -s $server_log ]; then
    echo "=== $server_log ==="
    cat $server_log
  fi
fi

rm ./.ncbi_test_con_pipename >/dev/null 2>&1

exit $exit_code
