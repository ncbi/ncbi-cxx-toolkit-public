#! /bin/sh
# $Id$

exit_code=0
client_log=test_ncbi_trigger_client.log
server_log=test_ncbi_trigger_server.log

rm -f $client_log $server_log

CONN_DEBUG_PRINTOUT=SOME;  export CONN_DEBUG_PRINTOUT

port="595`expr $$ % 100`"

test_ncbi_trigger -delay 20000 -port $port server >>$server_log 2>&1 &
spid=$!
test_ncbi_trigger              -port $port client >>$client_log 2>&1 &
sleep 1
cpid=$!
trap 'kill -9 $spid $cpid' 0 1 2 15

n="`expr $$ % 3`"
while kill -0 $cpid; do
  i=0
  while [ $i -le $n ]; do
    $CHECK_EXEC test_ncbi_socket localhost $port >/dev/null 2>&1 &
    n="`expr $! % 3`"
    i="`expr $i + 1`"
  done
  sleep 1
done

wait $spid
server_exit_code=$?

wait $cpid
client_exit_code=$?

test $server_exit_code != 0  &&  exit_code=$server_exit_code
test $client_exit_code != 0  &&  exit_code=$client_exit_code

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

exit $exit_code
