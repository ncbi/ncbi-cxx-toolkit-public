#! /bin/sh

# $Id$
# Author:  Denis Vakatov, NCBI 
#
###########################################################################
#
#  Auxilary script -- to be called by "./Makefile.check"
#
#  Command line:
#     check.sh <signature> [reporting args]
#
#  Reporting mode and addresses' args:
#     file:[full:]/abs_fname
#     file:[full:]rel_fname
#     mail:[full:]addr1,addr2,...
#     post:[full:]url
#     stat:addr1,addr2,...
#     watch:addr1,addr2,...
#     debug
#     sendonly
#
###########################################################################


####  ERROR STREAM

err_log="/tmp/check.sh.err.$$"
#exec 2>$err_log
trap "rm -f $err_log" 1 2 15

# The limit on the sending email size in Kbytes
mail_limit=199


####  INCLUDE COMMON.SH

script_name=`basename $0`
script_dir=`dirname $0`
script_dir=`(cd "${script_dir}" ; pwd)`
. ${script_dir}/../../scripts/common/common.sh


####  MISC

script_name=`basename $0`
script_args="$*"

summary_res="check.sh.log"
error_res="check.sh.out_err"

if test -x /usr/sbin/sendmail; then
    sendmail="/usr/sbin/sendmail -oi"
else
    sendmail="/usr/lib/sendmail -oi"
fi

# MIME headers intended to keep Outlook Exchange from mangling the body.
# Perhaps we should hardcode UTF-8 rather than ISO 8859-1, but it's a
# moot point since we'll generally just encounter ASCII.
DoMime()
{
    cat <<EOF
MIME-Version: 1.0
Content-Type: text/plain; charset="iso-8859-1"
Content-Transfer-Encoding: binary
EOF
}

####  ERROR REPORT

Error()
{
   cat <<EOF 1>&2

-----------------------------------------------------------------------

$script_name $script_args

ERROR:  $1
EOF

   cat $err_log

   kill $$
   exit 1
}

 
####  SPECIAL ARGUMENTS (DEBUG, SENDONLY)

debug="no"
need_check="yes"

for arg in "$@" ; do
   case "$arg" in
    debug )
      set -xv
      debug="yes"
      run_script="/bin/sh -xv"
      ;;
    sendonly )
      need_check="no"
      if test ! -f $summary_res -o ! -f $error_res ; then
         Error "Missing check result files"
      fi
      ;;
   esac
done


####  ARGS

signature="$1"
shift

# If "$need_check" = "no" that files with check results already
# prepared and standing in the current directory. 
# Only post results in this case.

if test "$need_check" = "yes" ; then
   # Default check
   test $# -ge 3  ||  Error "Wrong number of args:  $#"
   make="$1"
   builddir="$2"
   action="$3"

   shift
   shift
   shift

   test -d "$builddir"  &&  cd "$builddir"  ||  \
     Error "Cannot CHDIR to directory:  $builddir"

   case "$action" in
    all )
      # continue
      ;;
    clean | purge )
      test -x ./check.sh  &&  $run_script ./check.sh clean
      exit 0
      ;;
    * )
      Error "Invalid action: $action"
      ;;
   esac

   ####  RUN CHECKS

   export MAKEFLAGS
   MAKEFLAGS=
   "$make" check_r RUN_CHECK=N  ||  Error "MAKE CHECK_R failed"
   $run_script ./check.sh run
fi


####  POST RESULTS

# Parse the destination location list
for dest in "$@" ; do
   type=`echo "$dest" | sed 's%\([^:][^:]*\):.*%\1%'`
   loc=`echo "$dest" | sed 's%.*:\([^:][^:]*\)$%\1%'`

   full=""
   echo "$dest" | grep ':full:' >/dev/null  &&  full="yes"

   case "$type" in
    file )
      echo "$loc" | grep '^/' >/dev/null ||  loc="$builddir/$loc"
      loc=`echo "$loc" | sed 's%{}%'${signature}'%g'`
      if test -n "$full" ; then
         file_list_full="$file_list_full $loc"
      else
         file_list="$file_list $loc"
      fi
      ;;
    mail )
      if test -n "$full" ; then
         mail_list_full="$mail_list_full $loc"
      else
         mail_list="$mail_list $loc"
      fi
      ;;
    stat )
      stat_list="$stat_list $loc"
      ;;
    watch )
      loc=`echo "$loc" | sed 's/,/ /g'`
      watch_list="$watch_list $loc"
      ;;
    debug | sendonly )
      ;;
    * )
      err_list="$err_list BAD_TYPE:\"$dest\""
      ;;
   esac
done


# Compose "full" results archive, if necessary
if test "$need_check" = "yes" ; then
   $run_script ./check.sh concat_err
fi

# Post results to the specified locations
if test -n "$file_list_full" ; then
   for loc in $file_list_full ; do
      cp -p $error_res $loc  ||  \
         err_list="$err_list COPY_ERR:\"$loc\""
   done
fi

# Report check results to authors (Unix only)
if test "$need_check" = "yes" ; then
   if test "$NCBI_CHECK_MAILTO_AUTHORS." = 'Y.' ; then
      $run_script ./check.sh report_err
   fi
fi

if test -n "$file_list" ; then
   for loc in $file_list ; do
      cp -p $summary_res $loc  ||  \
          err_list="$err_list COPY_ERR:\"$loc\""
   done
fi

n_ok=`grep '^OK  --  '                   "$summary_res" | wc -l | sed 's/ //g'`
n_err=`grep '^ERR \[[0-9][0-9]*\] --  '  "$summary_res" | wc -l | sed 's/ //g'`
n_abs=`grep '^ABS --  '                  "$summary_res" | wc -l | sed 's/ //g'`

subject="${signature}  OK:$n_ok ERR:$n_err ABS:$n_abs"
tmp_src="/tmp/check.sh.$$.src"
tmp_dst="/tmp/check.sh.$$.dst"

if test -n "$mail_list_full" ; then
    {
      cat $summary_res
      echo ; echo '%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%' ; echo
      cat $error_res
    } > $tmp_src 
    COMMON_LimitTextFileSize $tmp_src $tmp_dst $mail_limit
    for loc in $mail_list_full ; do
      mailto=`echo "$loc" | sed 's/,/ /g'`
      {
        echo "To: $mailto"
        echo "Subject: [C++ CHECK]  $subject"
        DoMime
        echo
        echo "$subject"; echo
        cat $tmp_dst
      } | $sendmail $mailto  ||  err_list="$err_list MAIL_ERR:\"$loc\""
   done
fi

if test -n "$mail_list"  -a  -s "$error_res" ; then
   COMMON_LimitTextFileSize $error_res $tmp_dst $mail_limit
   for loc in $mail_list ; do
      mailto=`echo "$loc" | sed 's/,/ /g'`
      {
        echo "To: $mailto"
        echo "Subject: [C++ ERRORS]  $subject"
        DoMime
        echo
        echo "$subject"; echo
        cat $tmp_dst
      } | $sendmail $mailto  ||  err_list="$err_list MAIL_ERR:\"$loc\""
   done
fi

# Post check statistics
if test -n "$stat_list" ; then
   COMMON_LimitTextFileSize $summary_res $tmp_dst $mail_limit
   for loc in $stat_list ; do
      mailto=`echo "$loc" | sed 's/,/ /g'`
      {
        echo "To: $mailto"
        echo "Subject: [`date '+%Y-%m-%d %H:%M'`]  $subject"
        DoMime
        echo
        cat $tmp_dst
      } | $sendmail $mailto  ||  err_list="$err_list STAT_ERR:\"$loc\""
   done
fi

# Post errors to watchers
if test -n "$watch_list" ; then
   if test -n "$err_list"  -o  "$debug" = "yes" ; then
      {
        echo "To: $watch_list"
        echo "Subject: [C++ WATCH]  $signature"
        DoMime
        echo
        echo "$err_list"
        echo
        echo "========================================"
        COMMON_LimitTextFileSize $err_log $tmp_dst $mail_limit
        cat $tmp_dst
      } | $sendmail $watch_list
   fi
fi

# Cleanup
rm -f $tmp_src $tmp_dst $err_log >/dev/null 2>&1

exit 0
