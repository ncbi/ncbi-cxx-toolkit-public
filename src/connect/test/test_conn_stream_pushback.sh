#! /bin/sh
#
# $Id$
#

exit_code=0
log=test_conn_stream_pushback.log
rm -f $log

trap 'rm -f $log; echo "`date`."' 0 1 2 15

i=0
j="`expr $$ % 2 + 1`"
while [ $i -lt $j ]; do
  echo "${i} of ${j}: `date`"
  $CHECK_EXEC test_conn_stream_pushback >$log 2>&1
  exit_code=$?
  if [ "$exit_code" != "0" ]; then
    if [ "`wc -l $log`" -gt "40" ]; then
      cat -n $log | head -20
      echo '......'
      cat -n $log | tail -20
    else
      cat $log
    fi
    break
  fi
  i="`expr $i + 1`"
done
exit $exit_code
