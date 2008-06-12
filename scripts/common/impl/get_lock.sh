#!/bin/sh
dir=$1.lock

user=$REMOTE_USER
[ -z "$user" ] && user=$USER
[ -z "$user" ] && user=`whoami`

# XXX - try to detect and clean stale locks, either once or every iteration?

seconds=0
while [ "$seconds" -lt 900 ]; do
    if mkdir $dir >/dev/null 2>&1; then
        [ "$seconds" = 0 ] || echo
        echo $1    > "$dir/command"
        hostname   > "$dir/hostname"
        echo $2    > "$dir/pid"
        echo $user > "$dir/user"
        exit 0
    fi
    if [ "$seconds" = 0 ]; then
        if [ x`echo -n` = x-n ]; then
            n=''; c='\c'
        else
            n='-n'; c=''
        fi
        echo $n "Waiting for $dir$c" >&2
    fi
    sleep 5
    echo $n ".$c" >&2
    seconds=`expr $seconds + 5`
done

if test -f "$dir"; then
    # old-style lock
    echo
    cat "$dir"
else
    fmt -74 <<EOF

`cat $dir/user` appears to be running `cat $dir/command` in `pwd` as
process `cat $dir/pid` on `cat $dir/hostname`.  If you have received
this message in error, remove `pwd`/$dir and try again.
EOF
fi

exit 1
