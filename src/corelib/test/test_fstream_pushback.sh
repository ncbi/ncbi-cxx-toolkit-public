#! /bin/sh
#
# $Id$
#
exit_code=0
log=test_fstream_pushback.log
rm -f $log

trap 'rm -f $log; echo "`date`."' 0 1 2 15

i=0
j=`expr $$ % 2 + 1`
while [ $i -lt $j ]; do
  echo "${i} of ${j}: `date`"
  $CHECK_EXEC test_fstream_pushback >$log 2>&1
  exit_code=$?
  if [ "$exit_code" != "0" ]; then
    if [ "`head -n 301 $log | wc -l`" -gt "300" ]; then
      cat -n $log | head -100
      echo '......'
      cat -n $log | tail -200
    else
      cat $log
    fi
    break
  fi
  i="`expr $i + 1`"
done
exit $exit_code
