#! /bin/sh
# $Id$

outlog()
{
  logfile="$1"
  if [ -s "$logfile" ]; then
    echo "=== $logfile ==="
    if [ "`head -401 $logfile 2>/dev/null | wc -l`" -gt "400" ]; then
      head -100 "$logfile"
      echo '...'
      tail -300 "$logfile"
    else
      cat "$logfile"
    fi
  fi
}

exit_code=0
log=test_ncbi_conn.log
rm -f $log

trap 'echo "`date`."' 0 1 2 3 15

. ncbi_test_data

n="`ls -m $NCBI_TEST_DATA/proxy 2>/dev/null | wc -w`"
n="`expr ${n:-0} + 1`"
n="`expr $$ '%' $n`"

if [ -r $NCBI_TEST_DATA/proxy/test_ncbi_proxy.$n ]; then
  .     $NCBI_TEST_DATA/proxy/test_ncbi_proxy.$n
  proxy="$n"
fi

fw="`expr '(' $$ / 10 ')' '%' 2`"
if [ "$fw" = "1" ]; then
  CONN_FIREWALL=TRUE
  # Only proxy "0" is capable of forwarding the firewall port range
  case "`expr '(' $$ / 100 ')' '%' 3`" in
    0) test "$proxy" = "0"  &&  CONN_FIREWALL=FIREWALL
      ;;
    1)                          CONN_FIREWALL=FALLBACK
      ;;
    *)
      ;;
  esac
  export CONN_FIREWALL
elif [ -n "$proxy" ]; then
  if [ "$proxy" != "0" -o "`expr '(' $$ / 100 ')' '%' 2`" != "0" ]; then
    # Only proxy "0" is capable of forwarding the relay port range
    CONN_HTTP_PROXY_LEAK=TRUE
    export CONN_HTTP_PROXY_LEAK
  fi
fi

$CHECK_EXEC test_ncbi_conn -nopause 2>&1
exit_code=$?

test "$exit_code" = "0"  ||  outlog "$log"

exit $exit_code
