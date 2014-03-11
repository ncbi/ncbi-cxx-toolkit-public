#! /bin/sh
# $Id$

outlog()
{
  logfile="$1"
  if [ -s "$logfile" ]; then
    echo "=== $logfile ==="
    if [ "`head -401 $logfile 2>/dev/null | wc -l`" -gt "400" ]; then
      head -200 "$logfile"
      echo '...'
      tail -200 "$logfile"
    else
      cat "$logfile"
    fi
  fi
}

timeout=10
exit_code=0
port=test_ncbi_trigger.$$
server_log=test_ncbi_trigger_server.log
client_log=test_ncbi_trigger_client.log

rm -f $port $server_log $client_log

CONN_DEBUG_PRINTOUT=SOME;  export CONN_DEBUG_PRINTOUT

test_ncbi_trigger -delay 20000 -port $port server </dev/null >$server_log 2>&1 &
spid=$!
trap 'kill -9 $spid 2>/dev/null; rm -f $port; echo "`date`."' 0 1 2 3 15

t=0
while true; do
  if [ -s "$port" ]; then
    sleep 1
    $CHECK_EXEC test_ncbi_trigger -port "`cat $port`" client >$client_log 2>&1 &
    cpid=$!
    trap 'kill -9 $spid 2>/dev/null; kill -9 $cpid 2>/dev/null; rm -f $port; echo "`date`."' 0 1 2 3 15
    break
  fi
  t="`expr $t + 1`"
  if [ $t -gt $timeout ]; then
    echo "`date` FATAL: Timed out waiting on server to start." >$client_log
    exit_code=1
    break
  fi
  sleep 1
done

n="`expr $$ % 3`"
while kill -0 $cpid 2>/dev/null; do
  i=0
  while [ $i -le $n ]; do
    ( ulimit -c 0;  test_ncbi_socket localhost "`cat $port`" >/dev/null 2>&1 ) 2>/dev/null &
    n="`expr $! % 3`"
    i="`expr $i + 1`"
  done
  sleep 1
done

wait $cpid
client_exit_code=$?

wait $spid
server_exit_code=$?

test $server_exit_code != 0  &&  exit_code=$server_exit_code
test $client_exit_code != 0  &&  exit_code=$client_exit_code

if [ $exit_code != 0 ]; then
  outlog "$server_log"
  outlog "$client_log"
fi

exit $exit_code
