#! /bin/sh
# $Id$

outlog()
{
  logfile="$1"
  if [ -s "$logfile" ]; then
    echo "=== $logfile ==="
    if [ "`head -501 $logfile 2>/dev/null | wc -l`" -gt "500" ]; then
      head -200 "$logfile"
      echo '...'
      tail -300 "$logfile"
    else
      cat "$logfile"
    fi
  fi
}

exit_code=0
log=test_ncbi_ftp_download.log
rm -f $log

trap 'echo "`date`."' 0 1 2 3 15

# for netstat
PATH=${PATH}:/sbin:/usr/sbin

if [ -z "$proxy" -a "`netstat -a -n | grep -w 5556 | grep -c ':5556'`" != "0" ]; then
  n=2
else
  n=3
fi

case "`expr '(' $$ / 10 ')' '%' $n`" in
  0)
    file=
    ;;
  1)
    file='ftp://ftp.ncbi.nlm.nih.gov/'
    ;;
  2)
    file='ftp://ftp.kernel.org/pub/'
    ;;
esac

CONN_DEBUG_PRINTOUT=SOME;  export CONN_DEBUG_PRINTOUT

$CHECK_EXEC test_ncbi_ftp_download $file 2>&1
exit_code=$?

test "$exit_code" = "0"  ||  outlog "$log"

exit $exit_code
