#! /bin/sh
# $Id$
./test_ncbi_socket 5555 &
pid=$!
sleep 1
./test_ncbi_socket localhost 5555
exit=$?
kill $pid
exit $exit
