#!/bin/sh
case "$1" in
    --Debug   ) def_compress_others=yes; shift ;;
    --Release ) def_compress_others=no;  shift ;;
    *         ) def_compress_others= ;;
esac

compress='gzip -Nf'

ws='[ 	]' # Contains a space and a tab.
if [ -f ../../build_info ] \
    && (grep "^$ws*PURPOSE$ws*=$ws*FINAL$ws*\$" ../../build_info \
        &&  grep "^$ws*CODEBASE$ws*=$ws*PRODUCTION$ws*\$" ../../build_info \
        &&  bzip2 --version </dev/null) >/dev/null 2>&1; then
    # Some bzip2 releases still try to compress stdin with --version(!)
    compress='bzip2 -f'
fi

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
            plugin_test | speedtest | streamtest | biosample_chk \
                | testipub | test_basic_cleanup | test_checksum | test_mghbn \
                | test_ncbi_connutil_hit | test_ncbi_dblb | test_ncbi_http_get \
                | *.*z* )
                ;;
            *test* | *demo* | *sample* \
                | net*che*_c* | ns_*remote_job* | save_to_nc )
                $compress $f
                ;;
            *blast* | datatool | gbench* | id1_fetch | idwwwget | lbsmc \
                | one2all )
                ;;
            *)
                test "$compress_others" = "no" || $compress $f
                ;;
        esac
    done
done
