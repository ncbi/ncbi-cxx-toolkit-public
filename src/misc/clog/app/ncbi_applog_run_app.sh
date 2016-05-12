#!/bin/sh
#
# Wrapper script for an arbitrary application to report its calls to AppLog.
#
# USAGE: 
#     ncbi_applog_run_app sh <path_to_app> <arguments>
#


# Path to ncbi_applog utility
APPLOG='/opt/ncbi/bin/ncbi_applog-1'

# For debugging purposes uncomment 2 lines below. This will redirect
# all output to the current directory instead of sending it to AppLog.

#NCBI_CONFIG__NCBIAPPLOG_DESTINATION=cwd
#export NCBI_CONFIG__NCBIAPPLOG_DESTINATION


# --- Helper functions

# Error <message> [<exit_code>]
Error() {
    local exit_code=${2:-$?}
    echo "ncbi_applog error ($exit_code):" "$1"
    exit $exit_code
}

# UrlEncode <string>
UrlEncode() {
    local LANG=C
    local len="${#1}"
    for (( i = 0; i < len; i++ )); do
        local c="${1:i:1}"
        case $c in
            [a-zA-Z0-9.~_-]) printf "$c" ;;
            *) printf '%%%02X' "'$c" ;; 
        esac
    done
}


# --- Start

if test $# -lt 1; then
   echo "Usage: $0 <path_to_app> <arguments>"  &&  exit 1
fi

# Get an application name from the first argument (executable path)
appname=`echo "$1" | sed -e 's%.*/%%g' -e 's% %_%g'`

# Log starting, specifying application name and pid for our wrapping script.
# Getting token back, it is needed for all subsequent calls.

app_token=`$APPLOG start_app -appname "$appname" -pid "$$"`
status=$?
if [ $? -ne 0 -o -z "$app_token" ] ; then
    Error "start_app, token = $app_token" $status
fi


# --- Request

# Log user name and current directory.
# Each key/value in the parameters pairs for -param argument shoould be
# URL-encoded. 

params="user=`UrlEncode ${USER}`&pwd=`UrlEncode ${PWD}`"

# Start a request. It is possible don't use reguests at all and do logging
# on the application level only, but it is recommended to have at least
# one request to help AppLog to parse it correctly.

req_token=`$APPLOG start_request "$app_token" -param "$params"`
status=$?
if [ $? -ne 0 -o -z "$req_token" ] ; then
    Error "start_request, token = $req_token" $status
fi

# Log a command line as extra record.

cmdline=`UrlEncode "$*"`
$APPLOG extra "$req_token" -param "cmdline=${cmdline}"  ||  Error "extra"


# --- Execute an application

"$@"
app_exit=$?

# To log exit code correctly for AppLog, it is recommended to translate
# application's exit code to HTTP-like status code, where 200 mean OK (no error).

case $app_exit in
    0 ) log_app_exit=200 ;;
  200 ) log_app_exit=199 ;;
    * ) log_app_exit=$app_exit ;;
esac


# --- Stop

# Log stopping state for our request and application,

$APPLOG stop_request "$req_token" -status $log_app_exit  ||  Error "stop_request"
$APPLOG stop_app     "$app_token" -status $log_app_exit  ||  Error "stop_app"

exit $app_exit
