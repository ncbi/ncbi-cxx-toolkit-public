# $Id$
#!/bin/sh
#
# Check netschedule services

services="NS_test"
hosts="service0:9051"

res_file="/tmp/$0.$$"
trap 'rm -f $res_file' 1 2 15

n_ok=0
n_err=0
sum_list=''

for service in `echo $services $hosts`; do
   echo "-------------------------------------------------------"
   printf '%-40s' "Testing service '$service'"
   $CHECK_EXEC netschedule_check -q test $service > $res_file 2>&1
   if test $? -eq 0 ; then
      echo ":OK"
      n_ok=`expr $n_ok + 1`
   else
      echo ":ERR"
      cat $res_file
      n_err=`expr $n_err + 1`
   fi
done

rm -f $res_file

cat <<EOF

*******************************************************

SUCCEEDED:  $n_ok
FAILED:     $n_err

EOF

exit $n_err
