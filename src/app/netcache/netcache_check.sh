#!/bin/sh
# $Id$
#
# Check netcached services

services="`cat netcache_check_services.lst`"
test $? -eq 0 || exit 1

hosts="`cat netcache_check_servers.lst`"
test $? -eq 0 || exit 2

# For Nagios test

lbsmc='/opt/machine/lbsm/bin/lbsmc'
lbsm_feedback='/opt/machine/lbsm/sbin/lbsm_feedback'

stable_client="./netcache_check"
send_nsca="/home/ivanov/nagios/root/opt/machine/nagios/bin/send_nsca"
send_nsca_cfg="/home/ivanov/nagios/root/opt/machine/nagios/bin/send_nsca.cfg"
nagios_host="nagios"
service_host="NC_netcache"

res_file="/tmp/`basename $0`.$$"
trap 'rm -f $res_file' 1 2 15

# Send result to Nagios server

if test "$1" = "--nagios"; then
   if [ ! -x "$stable_client" ]; then
      echo "$stable_client not found"
      exit 1
   fi
   if [ ! -x "$send_nsca" ]; then
      echo "$send_nsca not found"
      exit 1
   fi
   for service in $services ; do
      $stable_client $service > $res_file 2>&1
      code=$?
# Format of information:
#    <host_name>[tab]<svc_descr>[tab]<ret_code>[tab]<output>[newline]
# Usage:
#    ./send_nsca -H <host> [-p port] [-to to_sec] [-d delim] [-c config_file]
      echo -e "$service_host\t$service\t$code\tnone" | \
         $send_nsca -H $nagios_host -c $send_nsca_cfg
   done
   rm -f $res_file
   exit 0
fi

# Otherwise, general testsuite check

n_ok=0
n_err=0
sum_list=''

os=`env | grep "OS=" | grep -i "Windows"`
if test $? -ne 0; then
    service_machines="service0 service1 service2 service3"
    mask="NC_.*"
    for s in $service_machines ; do
       list=`$lbsmc -g $s 0 2>&1 | tail +7 | cut -f1 -d' ' | sed '/-/{x;q;}' | grep -s "$mask"`
       test -n list  ||  list=""
       wholelist=`echo $wholelist $list` 
    done
    wholelist=`for s in $wholelist; do echo $s; done | sort | uniq`

    for s in $wholelist; do
        echo " $services " | grep " $s " 2>&1 >/dev/null
        test $? -ne 0 && missing_services=`echo $missing_services $s`
    done 
fi

for service in `echo $services $hosts`; do
   echo "-------------------------------------------------------"
   printf '%-40s' "Testing service '$service'"
   $CHECK_EXEC netcache_check $service > $res_file 2>&1
   if test $? -eq 0 ; then
      echo ":OK"
      n_ok=`expr $n_ok + 1`
#      sum_list="$sum_list _SEPARATOR_ +  $service"
   else
      echo ":ERR"
      cat $res_file
      n_err=`expr $n_err + 1`
#      sum_list="$sum_list _SEPARATOR_ -  $service"
   fi
done

rm -f $res_file

# Print summary
cat <<EOF

*******************************************************

SUCCEEDED:  $n_ok
FAILED:     $n_err
MISSING SERVICES: $missing_services

EOF
#echo "$sum_list" | sed 's/_SEPARATOR_/\
#/g'

exit $n_err
