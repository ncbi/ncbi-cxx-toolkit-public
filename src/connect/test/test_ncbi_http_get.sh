#! /bin/sh
# $Id$

CONN_DEBUG_PRINTOUT=SOME;  export CONN_DEBUG_PRINTOUT

. ncbi_test_data

n="`ls -m $NCBI_TEST_DATA/proxy 2>/dev/null | wc -w`"
n="`expr ${n:-0} + 1`"
n="`expr $$ '%' $n`"

if [ -r $NCBI_TEST_DATA/proxy/test_ncbi_proxy.$n ]; then
  .     $NCBI_TEST_DATA/proxy/test_ncbi_proxy.$n
  proxy="$n"
fi

if [ -r $NCBI_TEST_DATA/x.509/test_ncbi_http_get ]; then
  .     $NCBI_TEST_DATA/x.509/test_ncbi_http_get
fi

ssl="`expr '(' $$ / 10 ')' '%' 2`"
if [ "$ssl" = "1" -a "`echo $FEATURES | grep -vic '[-]GNUTLS'`" = "1" ]; then
  # for netstat
  PATH=${PATH}:/sbin:/usr/sbin
  CONN_USESSL=1
  CONN_GNUTLS_LOGLEVEL=2
  export PATH CONN_USESSL CONN_GNUTLS_LOGLEVEL
  if [ -z "$proxy" -a "`netstat -a -n | grep -w 5556 | grep -c ':5556'`" != "0" ]; then
    url='https://localhost:5556'
  else
    url='https://www.ncbi.nlm.nih.gov/Service/index.html'
  fi
else
  url='http://www.ncbi.nlm.nih.gov/entrez/viewer.cgi?view=0&maxplex=1&save=idf&val=4959943'
fi

exec test_ncbi_http_get "$url"
