#!/bin/sh
if [ "x$1" = "x-base" ]; then
    command=$2
    shift 2
else
    command=`basename $1`
fi

clean_up () {
    rm -rf "$command.lock"
}

case $0 in
    */*) get_lock="`dirname $0`/get_lock.sh" ;;
    *) get_lock=get_lock.sh ;;
esac

if "$get_lock" "$command" $$; then
    trap 'clean_up' 1 2 15
    "$@"
    status=$?
    clean_up
    exit $status
else
    exit 1
fi
