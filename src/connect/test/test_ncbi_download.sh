#! /bin/sh
# $Id$

fw="`expr $$ '%' 10`"
if [ "`expr $fw '%' 2`" = "1" ]; then
  : ${CONN_FIREWALL:=TRUE}
  export CONN_FIREWALL
fi

inhouse="`echo $FEATURES | grep -v '[-]in-house-resources' | grep -c 'in-house-resources'`"
if [ "$inhouse" != "0" -a "`expr $fw '%' 3`" = "0" ]; then
  : ${CONN_HOST:=test.ncbi.nlm.nih.gov}
  export CONN_HOST
fi

: ${CONN_MAX_TRY:=1}
export CONN_MAX_TRY

$CHECK_EXEC test_ncbi_download downtest
