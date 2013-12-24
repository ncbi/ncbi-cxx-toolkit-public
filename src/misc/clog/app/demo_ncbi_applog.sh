#!/bin/sh

APPLOG=${1:-"./ncbi_applog"}


Error() {
    echo 'ERROR::' "$1 failed"
    exit 1
}

SetOutput() {
    NCBI_CONFIG__NCBI__NcbiApplogDestination=$1
    export NCBI_CONFIG__NCBI__NcbiApplogDestination
}


# -- test environment
#NCBI_APPLOG_SITE=testlogsite
#export NCBI_APPLOG_SITE
#SetOutput cwd


# --- Start

pid=$$

token=`$APPLOG start_app -pid=$pid -appname="demo_ncbi_applog" -host=DEMOHOST -sid=DEMOSID`
if [ $? -ne 0 -o -z "$token" ] ; then
    Error "start_app"
fi

# --- Posting

$APPLOG post $token -severity info    -message "app info message"    || Error "post_app_info"
$APPLOG post $token -severity warning -message "app warning message" || Error "post_app_warn"

# Try to use logging token via $NCBI_APPLOG_TOKEN (the token parameter is empty below)
NCBI_APPLOG_TOKEN=$token
export NCBI_APPLOG_TOKEN

$APPLOG post  '' -message "app error message"                        || Error "post_app_err"
$APPLOG post  '' -severity trace -message "app trace message"        || Error "post_app_trace"
$APPLOG perf  '' -status=0 -time 1.2                                 || Error "perf_app_1"
$APPLOG perf  '' -status=404 -time=4.5 -param="k1=v1&k2=v2"          || Error "perf_app_2"
$APPLOG extra ''                                                     || Error "extra_app_1"
$APPLOG extra '' -param="extra1=1"                                   || Error "extra_app_2"

# --- Request 1

request_token=`$APPLOG start_request '' -sid=request1 -rid=1 -client=1.2.3.4 -param="r11=v1&r12=v2"`
if [ $? -ne 0 -o -z "$request_token" ] ; then
    Error "start_request(1)"
fi
$APPLOG post  $request_token -message "request error message"        || Error "post_request(1)_err"
$APPLOG post  $request_token -severity trace -message "request trace message" || Error "post_request(1)_trace"
$APPLOG perf  $request_token -time 3.4                               || Error "perf_request(1)_1"
$APPLOG perf  $request_token -status=505 -time=50.6 -param="name=r1" || Error "perf_request(1)_2"
$APPLOG extra $request_token -param="k3=v3"                          || Error "extra_request(1)"
$APPLOG stop_request $request_token -status=200 -input=11 -output=13 >/dev/null 2>&1 || Error "stop_request(1)"

# --- Posting between requests
$APPLOG post '' -message "message posted between requests"   || Error "post_app_between"

# --- Request 2
request_token=`$APPLOG start_request '' -sid=request2 -rid=2 -client=5.6.7.8 -param="r21=v3&r22=v4"`
if [ $? -ne 0 -o -z "$request_token" ] ; then
    Error "start_request(2)"
fi
$APPLOG extra $request_token -param="k4=v4" || Error "extra_request(2)"
token=`$APPLOG stop_request $request_token -status=600 -input=21 -output=23` || Error "stop_request(2)"

# --- Stop 
$APPLOG stop_app '' -status=99 || Error "stop_app"


echo "OK"

exit 0
