#! /bin/sh
# $Id$

# Fail immediately if any command fails.
set -e

# Run this unconditionally on all platforms
$CHECK_EXEC ncbi_dblb_cli lookup -service MAPVIEW

# Run more tests on linux hosts
case $CHECK_SIGNATURE in
    *linux* )
        $CHECK_EXEC test_ncbi_dblb_cli.py
        ;;
esac
