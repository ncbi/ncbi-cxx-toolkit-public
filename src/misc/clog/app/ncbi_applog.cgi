#!/bin/sh

NCBI_APPLOG=./ncbi_applog

# By default set log_site parameter to DEV (developer machine)
# for all request that use this CGI, if not specified otherwise.
# The -logsite parameter or $NCBI_LOG_SITE on a calling host,
# have priority over local defined $NCBI_LOG_SITE.

NCBI_LOG_SITE=dev
export NCBI_LOG_SITE

# Write to /log by default
#NCBI_CONFIG__NCBIAPPLOG_DESTINATION=stdlog
#export NCBI_CONFIG__NCBIAPPLOG_DESTINATION

echo Content-type: text/plain
echo ""

umask 002
read line
eval $NCBI_APPLOG $line -mode=cgi
