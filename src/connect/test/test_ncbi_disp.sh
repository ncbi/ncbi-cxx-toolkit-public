#! /bin/sh
# $Id$

case "`expr '(' $$ '%' 100 ')' '%' 4`" in
  0)
    service='bouncehttp'
    ;;
  1)
    service='bounce'
    ;;
  2)
    service='echo'
    ;;
  3)
    service='test'
    ;;
esac

lb="`expr '(' $$ / 100 ')' '%' 10`"
if [ "`expr $lb '%' 2`" = "0" ]; then
  : ${CONN_LB_DISABLE:=1}
  export CONN_LB_DISABLE
  case "`expr $lb '/' 2`" in
    0)  # 0
      : ${CONN_HTTP_USER_HEADER:='Client-Host: 1.1.1.1'}
      export CONN_HTTP_USER_HEADER
      ;;
    1)  # 2
      : ${CONN_EXTERNAL:=1}
      export CONN_EXTERNAL
      ;;
    *)  # 4,6,8
      ;;
  esac
fi

fw="`expr '(' $$ / 1000 ')' '%' 10`"
if [ "`expr $fw '%' 2`" = "1" ]; then
  CONN_FIREWALL=TRUE
  export CONN_FIREWALL
fi

$CHECK_EXEC test_ncbi_disp "$service"
