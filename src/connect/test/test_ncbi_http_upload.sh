#! /bin/sh
# $Id$

CONN_DEBUG_PRINTOUT=SOME;  export CONN_DEBUG_PRINTOUT

. ncbi_test_data

TEST_NCBI_HTTP_UPLOAD="$NCBI_TEST_DATA/http/test_ncbi_http_upload"

test -f "$TEST_NCBI_HTTP_UPLOAD"  ||  exit 0

$CHECK_EXEC test_ncbi_http_upload $@
