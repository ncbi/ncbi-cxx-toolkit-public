#!/usr/bin/awk -f
# $Id$
BEGIN { srcline=1; key="" }
/^#line/ { srcline=$2; split($3, tmp, "\""); src=tmp[2]; next }
/^[ 	]*[a-zA-Z0-9_]+[ 	]*=/ { key=$1 }
/^[ 	]*#define/ { key=$2 }
/^[ 	]*#[ 	]+define/ { key=$3 }
length(key) > 0 {
    if (seen[key]) {
        print src ":" srcline ": duplicate definition of " key
        print seen[key] ": original definition here"
    } else {
        seen[key] = src ":" srcline
    }
    key=""
}
{ ++srcline }
