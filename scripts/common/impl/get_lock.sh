#!/bin/sh

PATH=/bin:/usr/bin
export PATH

dir=$1.lock

host=$HOSTNAME
[ -z "$host" ] && host=$HOST
[ -z "$host" ] && host=`hostname`

user=$REMOTE_USER
[ -z "$user" ] && user=$USER
[ -z "$user" ] && user=$LOGNAME
[ -z "$user" ] && user=`whoami`

testfile=lock-test.$host.$2
clean_up() {
    rm -f "$testfile"
}

seconds=0
while [ "$seconds" -lt 900 ]; do
    if mkdir $dir >/dev/null 2>&1; then
        [ "$seconds" = 0 ] || cat >&2 <<EOF

Acquired `pwd`/$dir for PID $2 ($1)
EOF
        touch "$dir/for-$user@$host"
        echo $1    > "$dir/command"
        echo $host > "$dir/hostname"
        echo $2    > "$dir/pid"
        echo $user > "$dir/user"
        clean_up
        exit 0
    fi
    if [ "$seconds" = 0 ]; then
        if [ x`echo -n` = x-n ]; then
            n=''; c='\c'
        else
            n='-n'; c=''
        fi
        trap 'clean_up; exit 1' 1 2 15
        if (echo >$testfile) 2>/dev/null  &&  test -s $testfile; then
            echo $n "Waiting for `pwd`/$dir$c" >&2
        else
            if test -w .; then
                problem="free space"
            else
                problem="permissions"
            fi
            echo "Unable to create a lock in `pwd`; please check $problem." >&2
            clean_up
            exit 1
        fi
    elif [ -f "$dir/for-$user@$host" -a -s "$dir/pid" ]; then
        read old_pid < $dir/pid
        if kill -0 "$old_pid" >/dev/null 2>&1; then
            : # Keep waiting; evidently still alive
        else
            # Stale
cat >&2 <<EOF

Clearing stale lock `pwd`/$dir from PID $old_pid for PID $2 ($1)
EOF
            rm -rf "$dir"
            continue
        fi
    elif [ ! -f "$dir/command" -o ! -f "$dir/hostname" -o ! -f "$dir/pid" \
           -o ! -f "$dir/user" ]; then
        # Incomplete; wipe out if preexisting and at least a minute old.
        # Solaris's /bin/sh doesn't support test's -nt or -ot operators,
        # hence the use of ls and head.
        if [ $seconds = 60 ] \
           &&  [ `ls -dt $dir $testfile | head -n1` = $testfile ]; then
cat >&2 <<EOF

Clearing old incomplete lock `pwd`/$dir for PID $2 ($1)
EOF
            rm -rf "$dir"
            continue
        fi
    fi
    sleep 5
    echo $n ".$c" >&2
    seconds=`expr $seconds + 5`
done

clean_up

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
