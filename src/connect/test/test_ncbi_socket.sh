#! /bin/sh
# $Id$

outlog()
{
  logfile="$1"
  if [ -s "$logfile" ]; then
    echo "=== $logfile ==="
    if [ "`cat $logfile 2>/dev/null | wc -l`" -gt "200" ]; then
      head -100 "$logfile"
      echo '...'
      tail -100 "$logfile"
    else
      cat "$logfile"
    fi
  fi
}

exit_code=0
log=test_ncbi_socket.log
server_log=test_ncbi_socket_server.log
client_log=test_ncbi_socket_client.log

rm -f $log $server_log $client_log

test_ncbi_socket >$log 2>&1
exit_code=$?
if [ $exit_code != 0 ]; then
  outlog $log
  uptime
  exit $exit_code
fi

CONN_DEBUG_PRINTOUT=SOME;  export CONN_DEBUG_PRINTOUT

port="555`expr $$ % 100`"

test_ncbi_socket $port >$server_log 2>&1 &
spid=$!
trap 'kill -0 $spid 2>/dev/null && kill -9 $spid; echo "`date`."' 0 1 2 3 15

sleep 2
$CHECK_EXEC test_ncbi_socket localhost $port >$client_log 2>&1  ||  exit_code=1

( kill    $spid ) >/dev/null 2>&1  ||  exit_code=2
( kill -9 $spid ) >/dev/null 2>&1
wait $spid 2>/dev/null

if [ $exit_code != 0 ]; then
  outlog "$server_log"
  outlog "$client_log"
  uptime
fi

exit $exit_code
