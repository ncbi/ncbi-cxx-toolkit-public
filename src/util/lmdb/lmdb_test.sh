#! /bin/sh
# $Id$

testdb="./testdb"

exit_code=0
rm -rf $testdb
mkdir $testdb

$CHECK_EXEC lmdb_test1 >/dev/null 2>&1  ||  exit_code=$?
$CHECK_EXEC lmdb_test2 >/dev/null 2>&1  ||  exit_code=$?
$CHECK_EXEC lmdb_test3 >/dev/null 2>&1  ||  exit_code=$?
$CHECK_EXEC lmdb_test4 >/dev/null 2>&1  ||  exit_code=$?
$CHECK_EXEC lmdb_test5 >/dev/null 2>&1  ||  exit_code=$?

exit $exit_code
