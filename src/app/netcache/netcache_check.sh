#!/bin/sh
# $Id$
#
# Check netcached services

services="
   NC_FluAnalysis  \
   NC_Geo          \
   NC_gMap         \
   NC_MapView      \
   NC_NIHMS        \
   NC_PCAssayGraph \
   NC_PhyloTree    \
   NC_Sage         \
   NC_Splign       \
   NC_test         \
   service1:9001   \
   service2:9001   \
   service1:9005   \
   service2:9005   \
   service1:9009   \
   netcache:9009"

# For Nagios test

stable_client="./netcache_check"
send_nsca="/home/ivanov/nagios/root/opt/machine/nagios/bin/send_nsca"
send_nsca_cfg="/home/ivanov/nagios/root/opt/machine/nagios/bin/send_nsca.cfg"
nagios_host="nagios"
service_host="NC_netcache"

res_file="/tmp/$0.$$"
trap 'rm -f $res_file' 1 2 15

# Send result to Nagios server

if test "$1" == "--nagios"; then
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

for service in $services ; do
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

EOF
#echo "$sum_list" | sed 's/_SEPARATOR_/\
#/g'

exit $n_err
