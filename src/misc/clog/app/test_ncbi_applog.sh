#!/bin/sh

APPLOG=${1:-"./ncbi_applog"}

SERVER_PORT=
export SERVER_PORT


Error() {
    echo 'ERROR::' "$1"
    exit 1
}

SetOutput() {
    NCBI_CONFIG__NCBI__NcbiApplogDestination=$1
    export NCBI_CONFIG__NCBI__NcbiApplogDestination
}

Log() {
    cmd="$APPLOG $1"
    msg="$2"
    re="$3"

    SetOutput cwd
    eval $cmd >/dev/null 2>&1 || Error "$msg"
    SetOutput stdout
    eval $cmd | grep "$re" >/dev/null 2>&1 || Error "$msg (check)"
}


# Parts of regular expressions (for checks)
guid='[0-9A-Z]\{16\}'
tm='[0-9]\{4\}-[0-9]\{2\}-[0-9]\{2\}T[0-9]\{2\}:[0-9]\{2\}:[0-9]\{2\}\.[0-9]\{3\}'
std="$guid [0-9]\{4,\}/0000 $tm TESTHOST        UNK_CLIENT      TESTSID                  test_ncbi_applog"
req1="$guid [0-9]\{4,\}/0000 $tm TESTHOST        1\.2\.3\.4         request1                 test_ncbi_applog"
req2="$guid [0-9]\{4,\}/0000 $tm TESTHOST        5\.6\.7\.8         request2                 test_ncbi_applog"

# Cleanup
trap 'rm -f test_ncbi_applog.err test_ncbi_applog.log test_ncbi_applog.perf test_ncbi_applog.trace' 0 1 2 3 15


# ------------ Main -------------

# --- Start

SetOutput cwd
token=`$APPLOG start_app -pid=123 -appname="test_ncbi_applog" -host=TESTHOST -sid=TESTSID`
if [ $? -ne 0 -o -z "$token" ] ; then
    Error "start_app"
fi
echo "$token" | \
    grep "name=test_ncbi_applog&pid=123&guid=$guid&host=TESTHOST&asid=TESTSID&atime=[0-9]\{10,\}\.[0-9]\{1,\}" >/dev/null 2>&1 \
    || Error "Token have wrong format"
Log 'start_app -pid=123 -appname="test_ncbi_applog" -host=TESTHOST -sid=TESTSID' 'start_app' "^00123/000/0000/PB $std start"


# --- Posting

Log 'post $token -severity info -message "info message"'       'post(1)' "^00123/000/0000/P  $std Info: info message"
Log 'post $token -severity warning -message "warning message"' 'post(2)' "^00123/000/0000/P  $std Warning: warning message"

# Try to use logging token via $NCBI_APPLOG_TOKEN (the token parameter is empty below)
NCBI_APPLOG_TOKEN=$token
export NCBI_APPLOG_TOKEN

Log 'post  "" -message "error message"'                 'post(3)'  "^00123/000/0000/P  $std Error: error message"
Log 'post  "" -severity trace -message "trace message"' 'post(4)'  "^00123/000/0000/P  $std Trace: trace message"
Log 'perf  "" -time 1.2'                                'perf(1)'  "^00123/000/0000/P  $std perf          0 1.200000"
Log 'perf  "" -status=404 -time=4.5 -param="k1=1&k2=2"' 'perf(2)'  "^00123/000/0000/P  $std perf          404 4.500000 k1=1&k2=2"
Log 'extra ""'                                          'extra(1)' "^00123/000/0000/P  $std extra"
Log 'extra "" -param="extra1=1"'                        'extra(2)' "^00123/000/0000/P  $std extra         extra1=1"


# --- Request 1

SetOutput cwd
request_token=`$APPLOG start_request '' -sid=request1 -rid=1 -client=1.2.3.4 -param="r11=value1&r12=value2"`
if [ $? -ne 0 -o -z "$request_token" ] ; then
    Error "start_request(1)"
fi
echo "$request_token" | \
    grep "name=test_ncbi_applog&pid=123&guid=$guid&host=TESTHOST&asid=TESTSID&atime=[0-9]\{10,\}\.[0-9]\{1,\}&rid=1&rsid=request1&client=1\.2\.3\.4&rtime=[0-9]\{10,\}\.[0-9]\{1,\}" >/dev/null 2>&1 \
    || Error "Request 1 token have wrong format"

Log 'start_request $request_token -sid=request1 -rid=1 -client=1.2.3.4 -param="r11=value1&r12=value2"' 'start_request(1)' "^00123/000/0001/RB $req1 request-start r11=value1&r12=value2"
Log 'post          $request_token -message "request message"'       'post(5)'          "^00123/000/0001/R  $req1 Error: request message"
Log 'stop_request  $request_token -status=200 -input=11 -output=13' 'stop_request(1)'  "^00123/000/0001/RE $req1 request-stop  200 [0-9]\{1,\}.[0-9]\{3\} 11 13"


# --- Posting between requests

Log 'post "" -message "message between requests"' 'post(6)'  "^00123/000/0000/P  $std Error: message between requests"

# --- Request 2

SetOutput cwd
request_token=`$APPLOG start_request '' -sid=request2 -rid=2 -client=5.6.7.8 -param="r21=1&r22=2"`
if [ $? -ne 0 -o -z "$request_token" ] ; then
    Error "start_request(2)"
fi
Log 'start_request $request_token -sid=request2 -rid=2 -client=5.6.7.8 -param="r21=1&r22=2"' 'start_request(2)' "^00123/000/0002/RB $req2 request-start r21=1&r22=2"
Log 'stop_request  $request_token -status=600 -input=21 -output=23' 'stop_request(2)' "^00123/000/0002/RE $req2 request-stop  600 [0-9]\{1,\}\.[0-9]\{3\} 21 23"


# --- Stop 

Log 'stop_app "" -status=99' 'stop_app' "^00123/000/0000/PE $std stop          99 [0-9]\{1,\}\.[0-9]\{3\}"

echo "OK"

exit 0
