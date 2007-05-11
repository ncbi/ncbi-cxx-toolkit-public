#! /bin/sh
# $Id$

exit_code=0
port="575`expr $$ % 100`"
rm -f socket_io_bouncer.log test_ncbi_socket_connector.log

CONN_DEBUG_PRINTOUT=ALL; export CONN_DEBUG_PRINTOUT

socket_io_bouncer $port >>socket_io_bouncer.log 2>&1 &
server_pid=$!
trap 'kill -9 $server_pid' 0 1 2 15

sleep 1
$CHECK_EXEC test_ncbi_socket_connector localhost $port >>test_ncbi_socket_connector.log 2>&1  ||  exit_code=1

kill $server_pid  ||  exit_code=2
( kill -9 $server_pid ) >/dev/null 2>&1
if [ $exit_code != 0 ]; then
  if [ -s socket_io_bouncer.log ]; then
    echo '=== socket_io_bouncer.log ==='
    cat socket_io_bouncer.log
  fi
  if [ -s test_ncbi_socket_connector.log ]; then
    echo '=== test_ncbi_socket_connector.log ==='
    cat test_ncbi_socket_connector.log
  fi
fi

exit $exit_code
