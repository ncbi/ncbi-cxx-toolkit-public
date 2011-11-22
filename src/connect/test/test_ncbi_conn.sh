#! /bin/sh
# $Id$

outlog()
{
  logfile="$1"
  if [ -s "$logfile" ]; then
    echo "=== $logfile ==="
    if [ "`cat $logfile 2>/dev/null | wc -l`" -gt "300" ]; then
      head -100 "$logfile"
      echo '...'
      tail -200 "$logfile"
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

ext="`expr $$ '%' 3`"
if [ -r $NCBI_TEST_DATA/proxy/test_ncbi_proxy.$ext ]; then
  .     $NCBI_TEST_DATA/proxy/test_ncbi_proxy.$ext
  proxy=1
fi

fw="`expr '(' $$ / 10 ')' '%' 2`"
if [ "$fw" = "1" ]; then
  CONN_FIREWALL=TRUE
  # Only proxy "0" is capable of forwarding the firewall port range
  case "`expr '(' $$ / 100 ')' '%' 3`" in
    0) test "$ext" = "0"  &&  CONN_FIREWALL=FIREWALL
      ;;
    1)                        CONN_FIREWALL=FALLBACK
      ;;
    *)
      ;;
  esac
  export CONN_FIREWALL
elif [ "${proxy:-0}" = "1" ]; then
  if [ "$ext" != "0" -o "`expr '(' $$ / 100 ')' '%' 2`" != "0" ]; then
    # Only proxy "0" is capable of forwarding the relay port range
    CONN_HTTP_PROXY_FLEX=TRUE # FIXME: Remove (obsolete now)
    CONN_HTTP_PROXY_LEAK=TRUE
    export CONN_HTTP_PROXY_FLEX CONN_HTTP_PROXY_LEAK
  fi
fi

$CHECK_EXEC test_ncbi_conn -nopause 2>&1
exit_code=$?

if [ "$exit_code" != "0" ]; then
  outlog "$log"
  uptime
fi

exit $exit_code
