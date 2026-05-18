#! /bin/sh
# $Id$

DIAG_OLD_POST_FORMAT=
export DIAG_OLD_POST_FORMAT
NCBI_LOG_HIT_ID=
export NCBI_LOG_HIT_ID

$CHECK_EXEC test_ncbidiag_mt $@
exit $?
