#!/bin/sh

NCBI_APPLOG=./ncbi_applog

# By default set log_site parameter to DEV (developer machine)
# for all request that use this CGI, if not specified otherwise.
# The -logsite parameter or $NCBI_APPLOG_SITE on a calling host,
# have priority over local defined $NCBI_APPLOG_SITE.

NCBI_APPLOG_SITE=dev
export NCBI_APPLOG_SITE

# uncomment to debug this CGI
#NCBI_CONFIG__NCBIAPPLOG_DESTINATION=stdout
#export NCBI_CONFIG__NCBIAPPLOG_DESTINATION

echo Content-type: text/plain
echo ""

umask 002

read -r line
if [[ $line =~ ^RAW ]] ; then
   params="`echo $line | sed 's/^RAW//'`"
   eval $NCBI_APPLOG raw -file - $params -mode=cgi
else
   eval $NCBI_APPLOG $line -mode=cgi
fi
