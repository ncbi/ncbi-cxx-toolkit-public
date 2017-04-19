#! /bin/sh
# $Id$

. ./ncbi_test_data

TEST_NCBI_HTTP_UPLOAD="$NCBI_TEST_DATA/http/test_ncbi_http_upload"
export TEST_NCBI_HTTP_UPLOAD

if [ ! -f "$TEST_NCBI_HTTP_UPLOAD" ]; then
  echo "NCBI_UNITTEST_SKIPPED"
  exit 0
fi

: ${CONN_DEBUG_PRINTOUT:=SOME};  export CONN_DEBUG_PRINTOUT

$CHECK_EXEC ${CHECK_EXEC:-./}test_ncbi_http_upload $@
