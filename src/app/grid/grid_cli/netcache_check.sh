#!/bin/sh
# $Id$
#
# Check netcached services

res_file="/tmp/`basename $0`.$$"
trap 'rm -f $res_file' 1 2 15 0

svn cat --username svnread --password allowed https://svn.ncbi.nlm.nih.gov/repos_htpasswd/toolkit/trunk/internal/c++/src/internal/cppcore/netcache/netcache_check_services.lst | tr $'\r\n' ' ' >$res_file
test $? -eq 0 || exit 1
services="`cat $res_file`"

svn cat --username svnread --password allowed https://svn.ncbi.nlm.nih.gov/repos_htpasswd/toolkit/trunk/internal/c++/src/internal/cppcore/netcache/netcache_check_servers.lst | tr $'\r\n' ' ' >$res_file
test $? -eq 0 || exit 2
hosts="`cat $res_file`"

n_ok=0
n_err=0
sum_list=''

GRID_CLI_LOGIN_TOKEN=''
export GRID_CLI_LOGIN_TOKEN

for service in `echo $services $hosts`; do
   echo "-------------------------------------------------------"
   printf '%-40s' "Testing service '$service'"
   $CHECK_EXEC grid_cli --admin sanitycheck --nc $service > $res_file 2>&1
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
