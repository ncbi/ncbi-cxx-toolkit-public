#! /bin/sh
# $Id$

exit_code=0

./socket_io_bouncer 5151 &
server_pid=$!
trap 'kill $server_pid' 1 2 15

sleep 1
./test_ncbi_socket_connector localhost 5151  ||  exit_code=1

kill $server_pid  ||  exit_code=2
exit $exit_code
