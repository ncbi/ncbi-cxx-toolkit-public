#! /bin/sh
# $Id$

driver_list="ctlib dblib ftds"
server_list="MSSQL3 STRAUSS MOZART"

res_file="/tmp/$0.$$"
trap 'rm -f $res_file' 1 2 15

n_ok=0
n_err=0
sum_list=""


# Run one test (RunTest sql_command reg_expression) 
RunTest()
{
  sql="$1"
  reg_exp="$2"

  echo
  $cmd "$sql" > $res_file 2>&1

  if test $? -eq 0 ; then
    if grep "$reg_exp" $res_file > /dev/null 2>&1 ; then
      echo "OK:"
      grep "$reg_exp" $res_file | tail -1
      n_ok=`expr $n_ok + 1`
      sum_list="$sum_list XXX_SEPARATOR +  $cmd '$sql'"
      return
    fi
  fi

  # error occured
  n_err=`expr $n_err + 1`
  sum_list="$sum_list XXX_SEPARATOR -  $cmd '$sql'"

  cat $res_file
}


# Loop through all combinations of {driver, server, test}
for driver in $driver_list ; do
  cat <<EOF



******************* DRIVER:  $driver  ************************

EOF
  for server in $server_list ; do
    cat <<EOF


~~~~~~ SERVER:  $server  ~~~~~~~~~~~~~~~~~~~~~~~~
EOF

    cmd="./lang_query -d $driver -S $server -Q"
    RunTest 'select qq = 57.55 + 0.0033' '<ROW><qq>57\.5533<'
    RunTest 'select qq = 57 + 33' '<ROW><qq>90<'
    RunTest 'select qq = GETDATE()' '<ROW><qq>../../.... ..:..:..<'
    RunTest 'select name, type from sysobjects' '<ROW><name>'
  done
done

rm -f $res_file


# Print summary
cat <<EOF


*******************************************************

SUCCEEDED:  $n_ok
FAILED:     $n_err
EOF
echo "$sum_list" | sed 's/XXX_SEPARATOR/\
/g'


# Exit
exit $n_err
