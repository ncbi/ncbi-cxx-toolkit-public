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
server_log=test_ncbi_trigger_server.log
client_log=test_ncbi_trigger_client.log

rm -f $server_log $client_log

CONN_DEBUG_PRINTOUT=SOME;  export CONN_DEBUG_PRINTOUT

port="595`expr $$ % 100`"

test_ncbi_trigger -delay 20000 -port $port server >$server_log 2>&1 &
spid=$!

sleep 2
test_ncbi_trigger              -port $port client >$client_log 2>&1 &
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
  outlog "$server_log"
  outlog "$client_log"
fi

exit $exit_code
