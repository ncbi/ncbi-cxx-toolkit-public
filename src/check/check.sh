#! /bin/sh

# $Id$
# Author:  Denis Vakatov, NCBI 
#
###########################################################################
#
#  Auxilary script -- to be called by "./Makefile.check"
#`
#  Reporting mode and addresses' args:
#     file:[full:]/abs_fname
#     file:[full:]rel_fname
#     mail:[full:]addr1,addr2,...
#     post:[full:]url
#     watch:addr1,addr2,...
#     debug
#
###########################################################################


####  ERROR STREAM
err_log="/tmp/check.sh.err.$$"
exec 2>$err_log
trap "rm -f $err_log" 1 2 15


####  DEBUG

n_arg=0
for arg in "$@" ; do
   n_arg=`expr $n_arg + 1`
   test $n_arg -lt 5   &&  continue

   if test "$arg" = "debug" ; then
      set -xv
      debug="yes"
      run_script="/bin/sh -xv"
      break
   fi
done


####  MISC

script_name=`basename $0`
script_args="$*"

summary_res="check.sh.log"
error_res="check.sh.out"

mailx="mailx -r `whoami | cut -f1 -d' '`"


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


####  ARGS

test $# -gt 3  ||  Error "Wrong number of args:  $#"

signature="$1"
make="$2"
builddir="$3"
action="$4"

shift
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

"$make" check_r RUN_CHECK=N  ||  Error "MAKE CHECK_R failed"
$run_script ./check.sh run


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
    watch )
      loc=`echo "$loc" | sed 's/,/ /g'`
      watch_list="$watch_list $loc"
      ;;
    debug )
      ;;
    * )
      err_list="$err_list BAD_TYPE:\"$dest\""
      ;;
   esac
done

# Compose "full" results archive, if necessary
$run_script ./check.sh concat_err

# Post results to the specified locations
if test -n "$file_list_full" ; then
   for loc in $file_list_full ; do
      cp -p $error_res $loc  ||  \
         err_list="$err_list COPY_ERR:\"$loc\""
   done
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

subject="${signature}   OK:$n_ok ERR:$n_err ABS:$n_abs"

if test -n "$mail_list_full" ; then
   for loc in $mail_list_full ; do
      mailto=`echo "$loc" | sed 's/,/ /g'`
      {
        echo "$subject"; echo
        cat $summary_res
        echo ; echo '%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%' ; echo
        cat $error_res
      } | $mailx -s "[C++ CHECK RESULTS]  $subject" $mailto \
           ||  err_list="$err_list MAIL_ERR:\"$loc\""
   done
fi

if test -n "$mail_list"  -a  -s "$error_res" ; then
   for loc in $mail_list ; do
      mailto=`echo "$loc" | sed 's/,/ /g'`
      {
        echo "$subject"; echo
        cat $error_res
      } | $mailx -s "[C++ CHECK ERRORS]  $subject" $mailto \
           || err_list="$err_list MAIL_ERR:\"$loc\""
   done
fi

# Post errors to watchers
if test -n "$watch_list" ; then
   if test -n "$err_list"  -o  "$debug" = "yes" ; then
      { echo "$err_list"; echo; echo "====================";  cat $err_log; } \
       | $mailx -s "[C++ CHECK WATCH]  ${signature}" $watch_list
   fi
fi


# Cleanup
rm -f $err_log
exit 0
