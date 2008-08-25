#!/bin/sh
case "$1" in
    --Debug   ) def_compress_others=yes; shift ;;
    --Release ) def_compress_others=no;  shift ;;
    *         ) def_compress_others= ;;
esac

for dir in "$@"; do
    if [ -n "$def_compress_others" ]; then
        compress_others=$def_compress_others
    else
        case "$dir" in
            *Debug* ) compress_others=yes ;;
            *       ) compress_others=no  ;;
        esac
    fi
    for f in $dir/*; do
        case "`basename $f`" in
            speedtest | streamtest | testipub | test_checksum | test_mghbn \
                | *.gz )
                ;;
            *test* | *demo* | *sample*)
                gzip -Nf $f
                ;;
            *blast* | datatool | gbench* | id1_fetch | idwwwget | lbsmc \
                | one2all )
                ;;
            *)
                test "$compress_others" = "no" || gzip -Nf $f
                ;;
        esac
    done
done
