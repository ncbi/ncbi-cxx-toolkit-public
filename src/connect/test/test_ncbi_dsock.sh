#! /bin/sh
# $Id$

exit_code=0
port="555`expr $$ % 100`"

./test_ncbi_dsock server $port >/dev/null 2>&1 &
server_pid=$!
trap 'kill $server_pid' 1 2 15

sleep 1
./test_ncbi_dsock client $port  ||  exit_code=1

exit $exit_code
