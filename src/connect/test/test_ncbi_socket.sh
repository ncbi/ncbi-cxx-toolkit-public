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
port=test_ncbi_socket.$$
log=test_ncbi_socket.log
server_log=test_ncbi_socket_server.log
client_log=test_ncbi_socket_client.log

rm -f $log $port $server_log $client_log

test_ncbi_socket >$log 2>&1
exit_code=$?
if [ $exit_code != 0 ]; then
  outlog $log
  exit $exit_code
fi

CONN_DEBUG_PRINTOUT=SOME;  export CONN_DEBUG_PRINTOUT

test_ncbi_socket $port </dev/null >$server_log 2>&1 &
spid=$!
trap 'kill -9 $spid 2>/dev/null; rm -f $port; echo "`date`."' 0 1 2 3 15

t=0
while true; do
  if [ -s "$port" ]; then
    sleep 1
    $CHECK_EXEC test_ncbi_socket localhost "`cat $port`" >$client_log 2>&1  ||  exit_code=1
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

( kill    $spid ) >/dev/null 2>&1  ||  exit_code=2
( kill -9 $spid ) >/dev/null 2>&1
wait $spid 2>/dev/null

if [ $exit_code != 0 ]; then
  outlog "$server_log"
  outlog "$client_log"
fi

exit $exit_code
