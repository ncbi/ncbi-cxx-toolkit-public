#! /bin/sh
# $Id$

rm -f deployable_cgi.stdout

test -d share  ||  mkdir share  ||  exit 1
mv -f deployable_cgi.html share/ ||  exit 2

export REQUEST_METHOD='GET'
export QUERY_STRING='message=TestDeployableCgi'

$CHECK_EXEC deployable_cgi.cgi > deployable_cgi.stdout

test $? = 0  ||  exit 3

grep -q 'TestDeployableCgi' deployable_cgi.stdout  ||  exit 4

exit 0
