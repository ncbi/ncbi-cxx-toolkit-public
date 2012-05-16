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
log=test_conn_stream_pushback.log
rm -f $log

trap 'echo "`date`."' 0 1 2 3 15

i=0
j="`expr $$ % 2 + 1`"
while [ $i -lt $j ]; do
  i="`expr $i + 1`"
  echo "`date`: Test launch ${i} of ${j}"
  $CHECK_EXEC test_conn_stream_pushback >$log 2>&1
  exit_code=$?

  if [ "$exit_code" != "0" ]; then
    outlog "$log"
    break
  fi
done

exit $exit_code
