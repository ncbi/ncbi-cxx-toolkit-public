#!/bin/sh
# $Id$

act=false
cache_dir='.#SRC-cache'
files=Makefile.*.[la][ip][bp]
test=test

common=_`basename $PWD`_common

[ -d "$cache_dir" ]  ||  mkdir "$cache_dir"
(test Makefile.in -ef Makefile.in) 2>/dev/null  ||  test=/usr/bin/test

for x in $files; do
    if [ "x$x" = "x$files" ]; then
        # echo "No application or library makefiles found in $PWD."
        exit 0
    elif $test \! -f "$cache_dir/$x" -o "$cache_dir/$x" -ot "$x"; then
        awk -F= '{ sub("#.*", "") }
            /^[ 	]*(UNIX_)?SRC[ 	]*=.*/ {
                src = $2
                sub("^[ 	]*", "", src)
                while (sub("\\\\$", "", src)) {
                    print src
                    getline src
                    sub("^[ 	]*", "", src)
                }
                print src
            }' $x | fmt -w1 | sort -u > "$cache_dir/$x"
        act=true
    fi
done

if [ $act = true ]; then
    for x in $files; do
        for y in $files; do
            if [ "$x" \!= "$y" ]  &&  \
               [ -n "`comm -12 \"$cache_dir/$x\" \"$cache_dir/$y\"`" ]; then
                echo "$x $common" | \
                    sed -e 's/^Makefile\.\(.*\)\.[la][ip][bp] /\1 /'
                break
            fi
        done
    done > '.#lock-map'
fi

exit 0
