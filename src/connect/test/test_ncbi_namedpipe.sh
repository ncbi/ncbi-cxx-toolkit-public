#! /bin/sh
# $Id$

exit_code=0

test_ncbi_namedpipe server $$ &
server_pid=$!
trap 'kill -9 $server_pid' 1 2 15

sleep 4
$CHECK_EXEC test_ncbi_namedpipe client $$ ||  exit_code=1

kill $server_pid  ||  exit_code=2
( kill -9 $server_pid ) >/dev/null 2>&1

rm ./.ncbi_test_pipename$$ >/dev/null 2>&1

exit $exit_code
