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

if [ "`echo $FEATURES | grep -c -E '(^| )connext( |$)'`" != "1" ]; then
  n=5
else
  n=6
fi

case "`expr '(' $$ / 10 ')' '%' $n`" in
  0)
    url=
    ;;
  1)
    url='ftp://ftp.ubuntu.com/'
    ;;
  2)
    url='ftp://ftp.freebsd.org/'
    ;;
  3)
    url='ftp://ftp.hp.com/pub/ls-gFLR.txt.gz'
    # use features and only passive FTP mode (active is disabled there)
    :    ${CONN_USEFEAT:=1}  ${CONN_REQ_METHOD:=POST}
    export CONN_USEFEAT        CONN_REQ_METHOD
    ;;
  4)
    url='ftp://ftp.gnu.org/find.txt.gz'
    # use features
    :    ${CONN_USEFEAT:=1}
    export CONN_USEFEAT
    ;;
  5)
    url='ftp://ftp-ext.ncbi.nlm.nih.gov/'
    ;;
esac

:    ${CONN_MAX_TRY:=1}  ${CONN_DEBUG_PRINTOUT:=SOME}
export CONN_MAX_TRY        CONN_DEBUG_PRINTOUT

$CHECK_EXEC test_ncbi_ftp_download $url 2>&1
exit_code=$?

if [ "$exit code" != "0" ]; then
  if echo "$url" | grep -q 'ftp.hp.com'  &&  grep -qs '500 OOPS:' $log ; then
    # ftp.hp.com is known to often malfunction with the following error:
    # "500 OOPS: failed to open vsftpd log file:/opt/webhost/logs/vsftpd/vsftpd.log"
    echo "NCBI_UNITTEST_SKIPPED"
    exit
  fi
  outlog "$log"
fi

exit $exit_code
