#! /bin/sh
# $Id$

driver_list="ctlib dblib ftds"
server_list="MS_DEV2 STRAUSS MOZART"
server_mssql="MS_DEV2"

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
  $CHECK_EXEC $cmd "$sql" > $res_file 2>&1

  if test $? -eq 0 ; then
    if grep "$reg_exp" $res_file > /dev/null 2>&1 ; then
      echo "OK:"
      grep "$reg_exp" $res_file
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


# Check existence of the "dbapi_driver_check"
$CHECK_EXEC dbapi_driver_check
if test $? -ne 99 ; then
  echo "The DBAPI driver existence check application not found."
  echo
  exit 1
fi


# Loop through all combinations of {driver, server, test}
for driver in $driver_list ; do
  cat <<EOF



******************* DRIVER:  $driver ************************

EOF
  if $CHECK_EXEC dbapi_driver_check $driver ; then
    for server in $server_list ; do
      if test $driver = "ctlib"  -a  $server = $server_mssql ; then
         continue
      fi
      if test \( $driver = "ftds"  -o  $driver = "ftds7" \)  -a \
                 $server != $server_mssql ; then
         continue
      fi

      cat <<EOF


~~~~~~ SERVER:  $server ~~~~~~~~~~~~~~~~~~~~~~~~
EOF
      cmd="lang_query -d $driver -S $server -Q"
      RunTest 'select qq = 57.55 + 0.0033' '<ROW><qq>57\.5533<'
      RunTest 'select qq = 57 + 33' '<ROW><qq>90<'
      RunTest 'select qq = GETDATE()' '<ROW><qq>../../.... ..:..:..<'
      RunTest 'select name, type from sysobjects' '<ROW><name>'
    done

  else
    cat <<EOF

Driver not found.
EOF
  fi
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
