#!/bin/sh

module=$1
shift

if test "$module" = ""; then
    echo "Usage: $0 [module]"
    exit 1
fi

if test -f "$module.module"; then
    :
else
    echo "File $module.module not found"
    exit 1
fi

p="`pwd`"
mp=

while true; do
    d=`basename "$p"`
    p=`dirname "$p"`
    if test -d "$p/src" && test -d "$p/include" && test -f "$p/configure.in" &&
        test -f "$p/src/Makefile.module"; then
        break
    fi
    if test -z "$mp"; then
        mp="$d"
    else
        mp="$d/$mp"
    fi
    if test "$p" = "/"; then
        echo "Root directory not found"
        exit 1
    fi
done

DATATOOL=`ls -Ltr $p/*/bin/datatool $p/bin/datatool 2>/dev/null 2>/dev/null | tail -1`

gmake -f "$p/src/Makefile.module" "MODULE=$module" "MODULE_PATH=$mp" "top_srcdir=$p" "DATATOOL=$DATATOOL" "$@"
