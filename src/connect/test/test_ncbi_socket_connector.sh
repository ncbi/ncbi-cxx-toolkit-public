#! /bin/sh
# $Id$

exit_code=0
port="`echo $$|sed 's/./ &/g'|tr ' ' '\n'|tail -2|tr '\n' ' '`"
port="555`echo $port|sed 's/ //g'`"

./socket_io_bouncer $port &
server_pid=$!
trap 'kill $server_pid' 1 2 15

sleep 1
./test_ncbi_socket_connector localhost $port  ||  exit_code=1

kill $server_pid  ||  exit_code=2
exit $exit_code
