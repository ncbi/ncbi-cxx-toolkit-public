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

case "`expr '(' $$ / 10 ')' '%' 4`" in
  0)
  ssl="`expr '(' $$ / 100 ')' '%' 2`"
  if [ "$ssl" = "1" -a "`echo $FEATURES | grep -vic '[-]GNUTLS'`" = "1" ]; then
    # for netstat
    PATH=${PATH}:/sbin:/usr/sbin
    CONN_GNUTLS_LOGLEVEL=2;  export CONN_GNUTLS_LOGLEVEL
    if [ -z "$proxy" -a "`netstat -a -n | grep -w 5556 | grep -c ':5556'`" != "0" ]; then
      url='https://localhost:5556'
    else
      url='https://www.ncbi.nlm.nih.gov'
    fi
  else
    url='http://www.ncbi.nlm.nih.gov'
  fi
  ;;
  1)
  if [ "`echo ${CHECK_SIGNATURE:-Unknown} | grep -c '^MSVC'`" != "0" ]; then 
    url='file:////?/c:/windows/system32/drivers/etc/hosts'
  else
    url='file:///etc/hosts'
  fi
  ;;
  2)
  url='ftp://ftp.ncbi.nlm.nih.gov/README.ftp'
  ;;
  3)
  url='http://ftp.ncbi.nlm.nih.gov/README.ftp'
  ;;
esac

exec test_ncbi_conn_stream_mt -threads "`expr $$ % 10 + 2`" "$url"
