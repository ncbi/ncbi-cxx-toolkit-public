#! /bin/sh
# $Id$

exit_code=0

./socket_io_bouncer 5555 &
server_pid=$!
trap 'kill $server_pid' 1 2 15

sleep 1
./test_ncbi_socket_connector localhost 5555  ||  exit_code=1

kill $server_pid  ||  exit_code=2
exit $exit_code
