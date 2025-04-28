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
server_log=test_ncbi_namedpipe_server.log
client_log=test_ncbi_namedpipe_client.log

rm -f $server_log $client_log

sfx=$$
test_ncbi_namedpipe -suffix $sfx server </dev/null >$server_log 2>&1 &
spid=$!
trap 'kill -9 $spid 2>/dev/null; rm -f ./.ncbi_test_pipename_$sfx; echo "`date`."' 0 1 2 3 15

t=0
quit_code=3
while true; do
  if [ -s $server_log ]; then
    sleep 2
    $CHECK_EXEC test_ncbi_namedpipe -suffix $sfx client >$client_log 2>&1
    status=$?
    if   [ $status -eq 2 ]; then
      quit_code=0 ; sleep 2
    elif [ $status -ne 0 ]; then
      exit_code=1
    fi
    break
  fi
  t="`expr $t + 1`"
  if [ $t -gt $timeout ]; then
    echo "`date` FATAL:  Timed out waiting for server to start." >$client_log
    exit_code=2
    break
  fi
  sleep 1
done

( kill    $spid ) >/dev/null 2>&1  ||  exit_code=$quit_code
( kill -9 $spid ) >/dev/null 2>&1
wait $spid 2>/dev/null

if [ $exit_code != 0 ]; then
  outlog "$server_log"
  outlog "$client_log"
fi

exit $exit_code
