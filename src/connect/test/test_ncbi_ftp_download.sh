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

if [ "`echo $FEATURES | grep -c -E '(^| )NCBICRYPT( |$)'`" != "1" ]; then
  n=4
else
  n=5
fi

case "`expr '(' $$ / 10 ')' '%' $n`" in
  0)
    url=
    ;;
  1)
    # url='ftp://ftp.ubuntu.com/'
    url='ftp://mirrors.rit.edu/ubuntu-releases/'
    ;;
  2)
    url='ftp://ftp.freebsd.org/'
    ;;
  3)
    url='ftp://ftp.gnu.org/find.txt.gz'
    # use features
    :    ${CONN_USEFEAT:=1}
    export CONN_USEFEAT
    ;;
  4)
    url='ftp://ftp-ext.ncbi.nlm.nih.gov/'
    ;;
esac

if [ -n "$url" ]; then
    :    ${CONN_TIMEOUT:=10}
    export CONN_TIMEOUT
fi
:    ${CONN_MAX_TRY:=1}  ${CONN_DEBUG_PRINTOUT:=SOME}
export CONN_MAX_TRY        CONN_DEBUG_PRINTOUT

$CHECK_EXEC test_ncbi_ftp_download $url >$log 2>&1
exit_code=$?

if [ "$exit_code" != "0" ]; then
  outlog "$log"
fi

exit ${exit_code}
