#! /bin/sh
# $Id$
./socket_io_bouncer 5555 &
pid=$!
sleep 1
./test_ncbi_socket_connector localhost 5555
exit=$?
kill $pid
exit $exit
