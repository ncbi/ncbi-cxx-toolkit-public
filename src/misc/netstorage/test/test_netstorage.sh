#!/bin/sh
# $Id$

NCBI_CONFIG__filetrack__api_key='bqyMqDEHsQ3foORxBO87FMNhXjv9LxuzF9Rbs4HLuiaf2pHOku7D9jDRxxyiCtp2'
NCBI_CONFIG__filetrack__site='dev'
export NCBI_CONFIG__filetrack__api_key NCBI_CONFIG__filetrack__site

$CHECK_EXEC test_netstorage
