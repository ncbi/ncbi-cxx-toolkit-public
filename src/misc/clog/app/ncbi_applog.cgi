#!/bin/sh

NCBI_APPLOG=./ncbi_applog

echo Content-type: text/plain
echo ""

# Read command line from stdin
read line
cmdline="-mode=cgi $line"

umask 002
eval $NCBI_APPLOG $cmdline
