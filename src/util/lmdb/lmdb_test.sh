#! /bin/sh
# $Id$

testdb="./testdb"

exit_code=0
rm -rf $testdb
mkdir $testdb

exec lmdb_test$1
