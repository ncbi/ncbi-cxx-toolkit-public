#! /bin/sh

CONN_DEBUG_PRINTOUT=SOME;  export CONN_DEBUG_PRINTOUT

. ncbi_test_data

n="`ls -m $NCBI_TEST_DATA/proxy 2>/dev/null | wc -w`"
n="`expr ${n:-0} + 1`"
n="`expr $$ '%' $n`"

if [ -r $NCBI_TEST_DATA/proxy/test_ncbi_proxy.$n ]; then
  .     $NCBI_TEST_DATA/proxy/test_ncbi_proxy.$n
fi

case "`expr '(' $$ / 10 ')' '%' 2`" in
  0)
  ssl="`expr '(' $$ / 100 ')' '%' 2`"
  if [ "$ssl" = "1" -a "`echo $FEATURES | grep -vic '[-]GNUTLS'`" = "1" ]; then
    CONN_GNUTLS_LOGLEVEL=7;  export CONN_GNUTLS_LOGLEVEL
    url='https://www.ncbi.nlm.nih.gov'
  else
    url='http://www.ncbi.nlm.nih.gov'
  fi
  huge_tar="$url"/"geo/download/?acc=GSE1580&format=file"
  ;;
  1)
  huge_tar="ftp://ftp.ncbi.nlm.nih.gov/geo/series/GSE1nnn/GSE1580/suppl/GSE1580_RAW.tar"
  ;;
esac

. ./test_tar.sh
