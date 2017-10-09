#! /bin/sh
# $Id$

. ./ncbi_test_data

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
  if [ "$ssl" = "1" ]; then
    # for netstat
    PATH=${PATH}:/sbin:/usr/sbin
    : ${CONN_TLS_LOGLEVEL:=2};  export CONN_TLS_LOGLEVEL
    if [ -z "$proxy" -a "`netstat -a -n | grep -c ':\<5556\>'`" != "0" ]; then
      url='https://localhost:5556'
    else
      url='https://www.ncbi.nlm.nih.gov'
    fi
  else
    url='http://www.ncbi.nlm.nih.gov'
  fi
  ;;
  1)
  case "`echo ${CHECK_SIGNATURE:-Unknown}`" in
    MSVC*|VS*)
    url='file:////?/c:/windows/system32/drivers/etc/hosts'
    ;;
    *)
    url='file:///etc/hosts'
    ;;
  esac
  ;;
  2)
  url='ftp://ftp-ext.ncbi.nlm.nih.gov/README.ftp'
  ;;
  3)
  url='http://ftp-ext.ncbi.nlm.nih.gov/README.ftp'
  ;;
esac

: ${CONN_DEBUG_PRINTOUT:=SOME};  export CONN_DEBUG_PRINTOUT

$CHECK_EXEC test_ncbi_conn_stream_mt -threads "`expr $$ % 10 + 2`" "$url"
