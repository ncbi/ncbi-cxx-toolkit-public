#!/bin/sh

NCBI_APPLOG=./ncbi_applog.bin

echo Content-type: text/plain
echo ""

umask 002
read line
eval $NCBI_APPLOG $line -mode=cgi
