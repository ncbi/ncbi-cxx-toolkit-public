#! /bin/sh

. ./ncbi_test_data

n="`ls -m $NCBI_TEST_DATA/proxy 2>/dev/null | wc -w`"
n="`expr ${n:-0} + 1`"
n="`expr $$ '%' $n`"

if [ -r $NCBI_TEST_DATA/proxy/test_ncbi_proxy.$n ]; then
  .     $NCBI_TEST_DATA/proxy/test_ncbi_proxy.$n
fi

case "`expr '(' $$ / 10 ')' '%' 3`" in
  0)
  ssl="`expr '(' $$ / 100 ')' '%' 2`"
  if [ "$ssl" = "1" ]; then
    :    ${CONN_TLS_LOGLEVEL:=2} ${CONN_MBEDTLS_LOGLEVEL:=1}
    export CONN_TLS_LOGLEVEL       CONN_MBEDTLS_LOGLEVEL
    url='https://www.ncbi.nlm.nih.gov'
  else
    url='http://www.ncbi.nlm.nih.gov'
  fi
  huge_tar="$url"/"geo/download/?acc=GSE1580&format=file"
  ;;
  1)
  huge_tar="ftp://ftp-ext.ncbi.nlm.nih.gov/geo/series/GSE1nnn/GSE1580/suppl/GSE1580_RAW.tar"
  ;;
  2)
  huge_tar="http://ftp-ext.ncbi.nlm.nih.gov/geo/series/GSE1nnn/GSE1580/suppl/GSE1580_RAW.tar"
  ;;
esac

:    ${CONN_MAX_TRY:=1} ${CONN_DEBUG_PRINTOUT:=SOME}
export CONN_MAX_TRY       CONN_DEBUG_PRINTOUT

. ./test_tar.sh
