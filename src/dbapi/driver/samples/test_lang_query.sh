#! /bin/sh

driver_list="ctlib dblib ftds"
test_app="lang_query"


# Run one test (RunTest sql_command reg_expression) 
RunTest() {
  sql=$1
  reg_exp=$2
  tmp="/tmp/$0.$$"
  $cmd "$sql" > $tmp 2>&1
  cat $tmp
  grep "Can not load a driver" $tmp  ||  return
  egrep "$reg_exp" $tmp
  result=$?
  rm $tmp
  test $result -eq 0  ||  exit 1
}


for driver in $driver_list ; do
  echo
  echo "*************************************************************"
  echo "                          " $driver
  echo "*************************************************************"
  echo 
 
  cmd="./lang_query -d $driver -S MSSQL3 -Q"

  echo "**************************** 1 ******************************"
  echo 
  RunTest 'select qq = 57.55 + 0.0033' '^*.>57\.5533<'

  echo "**************************** 2 ******************************"
  echo 
  RunTest 'select qq = 57 + 33' '^*.>90<'

  echo "**************************** 3 ******************************"
  echo 
  RunTest 'select qq = GETDATE()' '<ROW><qq>../../.... ..:..:..<'

  echo "**************************** 4 ******************************"
  echo 
  RunTest 'select name, type from sysobjects' '<ROW><name>*.'
done

exit 0
