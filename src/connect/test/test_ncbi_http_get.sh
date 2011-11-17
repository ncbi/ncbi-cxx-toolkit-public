#! /bin/sh
# $Id$

CONN_DEBUG_PRINTOUT=SOME;  export CONN_DEBUG_PRINTOUT

ext="`expr $$ '%' 3`"
if   [ -r /am/ncbiapdata/test_data/proxy/test_ncbi_proxy.$ext ]; then
  .       /am/ncbiapdata/test_data/proxy/test_ncbi_proxy.$ext
  proxy=1
elif [ -r /cygdrive/z/test_data/proxy/test_ncbi_proxy.$ext ]; then
  .       /cygdrive/z/test_data/proxy/test_ncbi_proxy.$ext
  proxy=1
fi

ssl="`expr '(' $$ / 10 ')' '%' 2`"

if [ "$ssl" = "1" -a "`echo $FEATURES | grep -vic '[-]GNUTLS'`" = "1" ]; then
  CONN_USESSL=1
  CONN_GNUTLS_LOGLEVEL=7
  export CONN_USESSL CONN_GNUTLS_LOGLEVEL
  if [ "${proxy:-0}" != "1" -a "`netstat -a -n | grep -w 5556 | grep -c ':5556'`" != "0" ]; then
    url='https://localhost:5556'
  else
    url='https://www.ncbi.nlm.nih.gov'
  fi
else
  url='http://www.ncbi.nlm.nih.gov/entrez/viewer.cgi?view=0&maxplex=1&save=idf&val=4959943'
fi
exec test_ncbi_http_get "$url"
