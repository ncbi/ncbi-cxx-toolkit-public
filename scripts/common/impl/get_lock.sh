#!/bin/sh
file=$1.lock

user=$REMOTE_USER
[ -z "$user" ] && user=$USER
[ -z "$user" ] && user=`whoami`

fmt -74 > "$file.$$" <<EOF
$user appears to be running $1 in `pwd` as process $2 on `hostname`.  If
you have received this message in error, delete `pwd`/$file and try again.
EOF

# echo no | mv -i "$file.$$" "$file" # Doesn't work on Solaris. :-/
if [ -f "$file" ]; then
    if [ x`echo -n` = x-n ]; then
        n=''; c='\c'
    else
        n='-n'; c=''
    fi
    echo $n "Waiting for $file$c" >&2
    for x in 1 2 3 4 5; do
        sleep $x
        [ -f "$file" ] || break
        echo $n ".$c" >&2
    done
    echo >&2
fi
[ -f "$file" ] || mv "$file.$$" "$file" # Not (quite) atomic. :-/

# Try to work around a weird race condition observed on Mac OS X 10.4
if [ -f "$file.$$" ]; then
    sleep 1
fi

if [ -f "$file.$$" ]; then
    cat "$file" >& 2
    rm "$file.$$"
#    kill $2
    exit 1
fi
