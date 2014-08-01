#!/bin/sh
set -e
PATH=/bin:/usr/bin
export PATH
unset CDPATH

base=$1
search=`echo ${2-$LD_LIBRARY_PATH} | tr : ' '`
output=$base/path-`echo "$search" | cksum | tr '	 ' __`
mkdir -p $output/new
cd $output
rm -f new/*

if [ -f stamp2 ]; then
    outdated=no
    for d in $search; do
        if [ "$d/." -nt stamp1 ]; then
            outdated=yes
            break
        fi
    done
    if [ $outdated = no ]; then
        rmdir new
        pwd
        exit 0
    fi
fi

rm -f stamp2
touch stamp1

for d in $search; do
    for l in $d/*.dylib $d/*.so $d/*.so.*; do
        case $l in
            */\*.* ) ;;
            *      ) [ -f new/"`basename \"$l\"`" ]  ||  ln -s "$l" new/ ;;
        esac
    done
done

for x in *.*; do
    if [ ! -f "new/$x" ]; then
        rm -f "$x"
    elif [ "`ls -Lli \"$x\"`" = "`cd new  &&  ls -Lli \"$x\"`" ]; then
        rm -f "new/$x"
    else
        mv -f "new/$x" .
    fi
done

case "`echo new/*`" in
    new/\* ) ;;
    *      ) mv new/* . ;;
esac
touch stamp2
rmdir new
pwd
