#! /bin/sh
# $Id$

# Fail immediately if any command fails.
set -e

# Run this unconditionally on all platforms
$CHECK_EXEC test_ncbi_dblb MAPVIEW

# Run more tests on linux hosts
case $CHECK_SIGNATURE in
    *linux* )
        $CHECK_EXEC test_ncbi_dblb.py
        ;;
esac
