#! /bin/sh
# $Id$

echo "test" | $CHECK_EXEC_STDIN cgitest
exit $?
