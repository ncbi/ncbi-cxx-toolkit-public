#! /bin/sh
# $Id$

exit_code=0
port="585`expr $$ % 100`"

if [ -x /sbin/ifconfig ]; then
  if [ "`arch`" = "x86_64" ]; then
    mtu="`/sbin/ifconfig lo 2>&1 | grep 'MTU:' | sed 's/^.*MTU:\([0-9][0-9]*\).*$/\1/'`"
  fi
fi

test_ncbi_dsock server $port $mtu >/dev/null 2>&1 &
server_pid=$!
trap 'kill -9 $server_pid' 1 2 15

sleep 1
$CHECK_EXEC test_ncbi_dsock client $port $mtu  ||  exit_code=1
( kill -9 $server_pid ) >/dev/null 2>&1

exit $exit_code
