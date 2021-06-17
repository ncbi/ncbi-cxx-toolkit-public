#! /bin/sh
# $Id$

. ./ncbi_test_data

m="`ls -m $NCBI_TEST_DATA/proxy 2>/dev/null | wc -w`"
n="`expr ${m:-0} + 1`"
n="`expr $$ '%' $n`"

if [ -r $NCBI_TEST_DATA/proxy/test_ncbi_proxy.$n ]; then
  .     $NCBI_TEST_DATA/proxy/test_ncbi_proxy.$n
  proxy="$n"
fi

if [ -r $NCBI_TEST_DATA/x.509/test_ncbi_http_get ]; then
  .     $NCBI_TEST_DATA/x.509/test_ncbi_http_get
  test "`expr '(' $$ / 10 ')' '%' 2`" = "0"  &&  unset TEST_NCBI_HTTP_GET_CLIENT_CERT
fi

if [ -z "$CONN_HTTP11" -a "`expr $$ '%' 2`" = "1" ]; then
  CONN_HTTP11=1
  export CONN_HTTP11
fi

ssl="`expr '(' $$ / 100 ')' '%' 2`"

# for netstat
PATH=${PATH}:/sbin:/usr/sbin

if [ "$ssl" = "1" ]; then
  : ${CONN_USESSL:=1}
  : ${CONN_TLS_LOGLEVEL:=2}
  export PATH CONN_USESSL CONN_TLS_LOGLEVEL
  if [ -z "$proxy" -a "`netstat -a -n | grep -c ':\<5556\>'`" != "0" ]; then
    url='https://localhost:5556'
  else
    url='https://www.ncbi.nlm.nih.gov/Service/index.html'
  fi
elif [ "$m" = "0" -o -n "$CONN_HTTP_USER_HEADER" -o \
       "`echo $FEATURES | grep -c -E '(^| )connext( |$)'`" != "1" ]; then
  : ${CONN_USESSL:=1};  export CONN_USESSL
  url='http://www.ncbi.nlm.nih.gov/Service/index.html'
else
  url='http://intranet.ncbi.nlm.nih.gov/Service/index.html'
fi

: ${CONN_DEBUG_PRINTOUT:=SOME};  export CONN_DEBUG_PRINTOUT

$CHECK_EXEC test_ncbi_http_get "$url"
