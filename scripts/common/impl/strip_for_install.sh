#!/bin/sh

case "$1" in
    --dirs )
        shift
        signature="$1"
        shift
        if test "$signature" = "workshop-Debug"; then
            find "$@" -name SunWS_cache -prune -o \
	        \( -type f -size +64 ! -name '*.o' -print \) | xargs rm -f
        else
            rm -rf "$@"
        fi
        ;;

    --bin )
        top_srcdir=$2
        bindir=$3
        dbindir=$bindir/.DEATH-ROW
        apps_to_drop=$top_srcdir/scripts/internal/projects/apps_to_drop

        if [ -f "$apps_to_drop" ]; then
            while read app; do
	        test -f "$bindir/$app" || continue
	        mkdir -p "$dbindir" && \
	            echo "mv .../bin/$app .../bin/.DEATH-ROW" && \
	            mv "$bindir/$app" "$dbindir"
            done < "$apps_to_drop"
        fi
        ;;

    --lib )
        top_srcdir=$2
        libdir=$3
        status_dir=$4
        dlibdir=$libdir/.DEATH-ROW
        libs_to_drop=$top_srcdir/scripts/internal/projects/libs_to_drop
        if [ -f "$libs_to_drop" ]; then
            while read lib; do
	        test -f "$status_dir/.$lib.dep" || continue
	        mkdir -p "$dlibdir" && \
	            echo "mv .../lib/lib$lib.* .../lib/.DEATH-ROW" && \
	            mv "$libdir/lib$lib.*" "$dlibdir"
	        test -f "$status_dir/.$lib-static.dep" || continue
	        mv $libdir/lib$lib-static.* "$dlibdir"
            done < "$libs_to_drop"
        fi
        ;;

    * )
        echo "$0: unknown mode $1"
        exit 1
        ;;
esac
