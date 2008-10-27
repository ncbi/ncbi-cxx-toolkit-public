#! /bin/sh
# $Id$

exit_code=0

test_ncbi_namedpipe server $$ &
spid=$!
trap 'kill -9 $spid; rm -f ./.ncbi_test_pipename_$$' 0 1 2 15

sleep 10
$CHECK_EXEC test_ncbi_namedpipe client $$  ||  exit_code=1

kill $spid  ||  exit_code=2
( kill -9 $spid ) >/dev/null 2>&1

rm -f ./.ncbi_test_pipename_$$ >/dev/null 2>&1

exit $exit_code
