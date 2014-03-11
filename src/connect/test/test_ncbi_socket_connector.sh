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
port=socket_io_bouncer.$$
server_log=socket_io_bouncer.log
client_log=test_ncbi_socket_connector.log

rm -f $port $server_log $client_log

CONN_DEBUG_PRINTOUT=SOME;  export CONN_DEBUG_PRINTOUT

# NB: socket_io_bouncer opens the log, too
socket_io_bouncer $port </dev/null >>$server_log 2>&1 &
spid=$!
trap 'kill -9 $spid 2>/dev/null; rm -f $port; echo "`date`."' 0 1 2 3 15

t=0
while true; do
  if [ -s "$port" ]; then
    sleep 1
    $CHECK_EXEC test_ncbi_socket_connector localhost "`cat $port`" >>$client_log 2>&1  ||  exit_code=1
    break
  fi
  t="`expr $t + 1`"
  if [ $t -gt $timeout ]; then
    echo "`date` FATAL: Timed out waiting on server to start." >>$client_log
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
