#!/bin/sh

PATH=/bin:/usr/bin:/usr/ccs/bin
export PATH

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

    --post-mortem )
        shift
        signature="$1"
        shift
        find "$@" -name 'make_*.log' | sed -e 's,/make_[^/]*\.log$,,' | \
            while read d; do
                for d2 in . `sort -u $d/.*.dirs 2>/dev/null`; do
                    test -f $d/$d2/ls-Al  &&  continue
                    listing=`ls -Al $d/$d2`
                    test "$signature" = "workshop-Debug" || \
                        symbols=`nm $d/$d2/*.o 2>/dev/null`
                    for x in $d/$d2/*; do
                        test -d "$x"  &&  continue
                        case "$x" in
                            *.o )
                                test "$signature" = "workshop-Debug" || \
                                    rm -f "$x"
                                ;;
                            *.[0-9] | *.*cgi | *.a | *.dylib | *.so )
                                rm -f "$x"
                                ;;
                            $d/$d2/*.* | */Makefile | */ls-Al | */symbols )
                                ;;
                            * )
                                rm -f "$x"
                                ;;
                        esac
                    done
                    echo "$listing" > $d/$d2/ls-Al
                    test -z "$symbols"  || \
                        echo "$symbols" | gzip > $d/$d2/symbols.gz
                done
            done
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
