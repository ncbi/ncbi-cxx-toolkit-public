#! /bin/sh
# $Id$

DIAG_OLD_POST_FORMAT=
export DIAG_OLD_POST_FORMAT

$CHECK_EXEC test_ncbidiag_mt $@
exit $?
