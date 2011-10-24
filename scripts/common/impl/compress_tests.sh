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
        [ -f "$f" ]  ||  continue
        case "`basename $f`" in
            plugin_test | speedtest | streamtest \
                | testipub | test_basic_cleanup | test_checksum | test_mghbn \
                | test_ncbi_connutil_hit | test_ncbi_dblb | test_ncbi_http_get \
                | *.gz )
                ;;
            *test* | *demo* | *sample* \
                | net*che*_c* | ns_*remote_job* | save_to_nc )
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
