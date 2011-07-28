#!/bin/sh
id='$Id$'

PATH=/bin:/usr/bin
export PATH

exec > auto_flat_tuneups.mk

cat <<EOF
# This file was generated on `date`
# by $id
EOF

IFS="$IFS;"
while read l; do
    case "$l" in
        *.*.real\ :*)
            set _ $l
            target=$2
            ;;
        *cd\ *MAKE*\ sources)
            set _ $l
            echo "$3 1 $target"
            ;;
        *cd\ *MAKE*_PROJ=*)
            set _ $l
            echo "$3 2 $target $2"
            ;;
        ptb_all.real\ :* )
            echo '~~~'
            ;;
    esac
done < Makefile.flat | sort | \
while read dir phase target rest; do
    if [ "$dir" \!= "$last_dir" ]; then
        if [ -n "$last_dir" ]; then
            if [ -n "$spec" ]; then
                echo "$last_dir.files.real: $spec ;"
                echo "spec_bearing_dirs += $last_dir"
            else
                echo "${type}_dirs += $last_dir"
            fi
        fi
        [ -n "$phase" ]  ||  continue
        echo
        spec=''
        type=expendable
    fi
    case "$phase:$spec:$rest" in
        1::* )
            echo "$target: override MAKE := \$(DO_MAKE)"
            spec=$target
            ;;
        1:* )
            echo "$target: override MAKE := : \$(MAKE)"
            echo "$target: $spec";
            ;;
        2::*-*cd)
            echo "$target: $dir.files"
            ;;
        2::* )
            echo "$target: $dir.files"
            type=plain
            ;;
        2:* )
            echo "$target: $spec"
            ;;
    esac
    last_dir=$dir
done

exit 0
