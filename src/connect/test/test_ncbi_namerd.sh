#! /bin/sh
# $Id$

. ./ncbi_test_data

$CHECK_EXEC test_ncbi_namerd -f $NCBI_TEST_DATA/connect/test_ncbi_namerd_tests.json
