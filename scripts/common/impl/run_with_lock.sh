#!/bin/sh
# $Id$

orig_PATH=$PATH
PATH=/bin:/usr/bin
export PATH

base=
logfile=
map=
mydir=`dirname $0`
error_status=1

while :; do
    case "$1" in
        -base ) base=$2; shift 2 ;;
        -log  ) logfile=$2; shift 2 ;;
        -map  ) map=$2; shift 2 ;;
        \!    ) error_status=0; shift ;;
        *     ) break ;;
    esac
done
: ${base:=`basename "$1"`}

clean_up () {
    /bin/rm -rf "$base.lock"
}

case $0 in
    */*) get_lock="$mydir/get_lock.sh" ;;
    *) get_lock=get_lock.sh ;;
esac

if [ -f "$map" ]; then
    while read old new; do
        if [ "x$base" = "xmake_$old" ]; then
            echo "$0: adjusting base from $base to make_$new per $map."
            base=make_$new
            break
        fi
    done < "$map"
fi

if "$get_lock" "$base" $$; then
    trap "clean_up; exit $error_status" 1 2 15
    if [ -n "$logfile" -a -z "$TEAMCITY_PROJECT_NAME" ]; then
        status_file=$base.lock/status
        (PATH=$orig_PATH; export PATH; "$@"; echo $? > "$status_file") 2>&1 \
            | tee "$logfile.new"
        # Emulate egrep -q to avoid having to move from under scripts.
        if [ ! -f "$logfile" ]  \
          ||  $mydir/is_log_interesting.awk "$logfile.new"; then
            mv -f "$logfile.new" "$logfile"
        fi
        if [ -s "$status_file" ]; then
            status=`tr -d '\n\r' < "$status_file"`
        else
            status=1
        fi
    else
        PATH=$orig_PATH
        export PATH
        "$@"
        status=$?
    fi
    clean_up
    case "$status:$error_status" in
        0:0 ) exit 1 ;;
        *:0 ) exit 0 ;;
        *   ) exit $status ;;
    esac
else
    exit $error_status
fi
