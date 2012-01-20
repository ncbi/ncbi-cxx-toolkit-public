#! /bin/sh
# $Id$

CONN_DEBUG_PRINTOUT=SOME;  export CONN_DEBUG_PRINTOUT

. ncbi_test_data

ext="`expr $$ '%' 3`"
if [ -r $NCBI_TEST_DATA/proxy/test_ncbi_proxy.$ext ]; then
  .     $NCBI_TEST_DATA/proxy/test_ncbi_proxy.$ext
  proxy=1
fi


case "`expr '(' $$ / 10 ')' '%' 3`" in
  0)
  ssl="`expr '(' $$ / 100 ')' '%' 2`"
  if [ "$ssl" = "1" -a "`echo $FEATURES | grep -vic '[-]GNUTLS'`" = "1" ]; then
    CONN_GNUTLS_LOGLEVEL=7;  export CONN_GNUTLS_LOGLEVEL
    if [ "${proxy:-0}" != "1" -a "`netstat -a -n | grep -w 5556 | grep -c ':5556'`" != "0" ]; then
      url='https://localhost:5556'
    else
      url='https://www.ncbi.nlm.nih.gov'
    fi
  else
    url='http://www.ncbi.nlm.nih.gov'
  fi
  ;;
  1)
  url='file:///etc/hosts'
  ;;
  2)
  url='ftp://ftp.ncbi.nlm.nih.gov/README.ftp'
  ;;
esac

exec test_ncbi_conn_stream_mt -threads "`expr $$ % 10 + 2`" "$url"
