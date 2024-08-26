#! /bin/sh
#$Id$

status_dir="$CFG_LIB/../status"
if test ! -d "$status_dir"; then
    status_dir="$CFG_LIB/../../status"
fi
if test ! -d "$status_dir"; then
    status_dir="../../../../../status"
fi

disabled() {
    if test -f "$status_dir/$1.enabled"; then
        return 1
    fi
    case "$FEATURES" in
        *" $1 "*) return 1;;
    esac
    return 0;
}

only_id2_test=0
no_id2_test=0
only_vdb_wgs_test=0
no_vdb_wgs_test=0
no_cache=0

while true; do
    case "$1" in
	-id2)
	    only_id2_test=1
	    shift
	    ;;
	-xid2|-no-id2)
	    no_id2_test=1
	    shift
	    ;;
	-vdb-wgs)
	    only_vdb_wgs_test=1
	    shift
	    ;;
	-no-vdb-wgs)
	    no_vdb_wgs_test=1
	    shift
	    ;;
	-no-cache)
	    no_cache=1
	    shift
	    ;;
	*)
	    break
	    ;;
    esac
done

methods=""
if test "$only_id2_test" = 0 -a "$only_vdb_wgs_test" = 0; then
    if disabled PubSeqOS; then
        echo "Skipping PUBSEQOS loader test (loader unavailable)"
        methods="$methods ID1"
    elif disabled in-house-resources; then
        echo "Skipping PUBSEQOS loader test (in-house resources unavailable)"
        methods="$methods ID1"
    else
        methods="$methods PUBSEQOS ID1"
    fi
fi
if test "$no_id2_test" = 0 -a "$no_vdb_wgs_test" = 0; then
    methods="$methods ID2"
fi
if test "$ONLY_PSG" = 1; then
    methods=""
fi
if test "$no_id2_test" = 1 -o "$no_vdb_wgs_test" = 1 || disabled PSGLoader || disabled in-house-resources; then
    echo "Skipping PSG loader test"
else
    methods="$methods PSG"
fi

if disabled DLL_BUILD; then
    # enable dll plugins for ftds and bdb
    NCBI_LOAD_PLUGINS_FROM_DLLS=1
    export NCBI_LOAD_PLUGINS_FROM_DLLS
fi

# netcache port on localhost
case "$NCBI_CONFIG_OVERRIDES" in
    *.netcache)
    f=~/"$NCBI_CONFIG_OVERRIDES"
    nc="localhost:9135"
    ncs="ids blobs"
    if test -r "$f"; then
        h=`grep '^host *=' $f | sed 's/.*= *//'`
        p=`grep '^port *=' $f | sed 's/.*= *//'`
        if  test -n "$h" && test -n "$p"; then
            nc="$h:$p"
        fi
        s=`grep '^service *=' $f | sed 's/.*= *//'`
        if  test -n "$s"; then
            nc="$s"
        fi
        a=`grep '^name *= *ids' $f | sed 's/.*= *//'`
        b=`grep '^name *= *blobs' $f | sed 's/.*= *//'`
        if  test -z "$a"; then
            a=ids
        fi
        if  test -z "$b"; then
            b=blobs
        fi
        ncs="$a $b"
    fi
    ;;
    *)
    nc="";;
esac

test_name=`basename "$1"`
base_cache_dir="./$test_name.$$.genbank_cache"
unset cache_dir

remove_cache() {
    if test -d "$cache_dir"; then
	rm -rf "$cache_dir"
    fi
}

init_cache() {
    if test -n "$nc"; then
        echo "Init netcache $nc/$ncs"
        for c in $ncs; do
            if grid_cli --admin purge --nc "$nc" --auth netcache_admin $c; then
                :
            else
                nc=""
                break
            fi
        done
        if test -n "$nc"; then
            return 0
        fi
        unset NCBI_CONFIG_OVERRIDES
    fi
    if disabled BerkeleyDB; then
        echo BerkeleyDB is not enabled
        return 1
    fi
    if disabled MT; then
        echo MT is not enabled
        return 1
    fi
    if disabled DLL; then
        echo No DLL plugins
        return 1
    fi
    cache_dir="$base_cache_dir"
    NCBI_CONFIG__GENBANK_SLASH_CACHE_SLASH_BLOB_CACHE_SLASH_BDB__PATH="$cache_dir"
    NCBI_CONFIG__GENBANK_SLASH_CACHE_SLASH_ID_CACHE_SLASH_BDB__PATH="$cache_dir"
    export NCBI_CONFIG__GENBANK_SLASH_CACHE_SLASH_BLOB_CACHE_SLASH_BDB__PATH
    export NCBI_CONFIG__GENBANK_SLASH_CACHE_SLASH_ID_CACHE_SLASH_BDB__PATH
    echo "Init BDB cache in $cache_dir"
    remove_cache
    trap "rm -rf \"$cache_dir\"" 0 1 2 15
    return 0
}

exitcode=0
failed=''
for method in $methods; do
    caches="1"
    GENBANK_LOADER_METHOD_BASE="$method"
    export GENBANK_LOADER_METHOD_BASE
    if test "$method" = "PSG"; then
	GENBANK_LOADER_PSG=1
	unset GENBANK_LOADER_METHOD_BASE
    else
	GENBANK_LOADER_PSG=0
	if test "$no_cache" = 0; then
	    caches="1 2 3"
	fi
    fi
    export GENBANK_LOADER_PSG
    for cache in $caches; do
        GENBANK_ALLOW_INCOMPLETE_COMMANDS=1
        export GENBANK_ALLOW_INCOMPLETE_COMMANDS
        if test "$cache" = 1; then
            m="$method"
            unset GENBANK_ALLOW_INCOMPLETE_COMMANDS
        elif test "$cache" = 2; then
            if init_cache; then
                :
            else
                echo "Skipping cache test"
                break
            fi
            m="cache;$method"
        else
            m="cache"
        fi
	if test "$m" = "$method"; then
	    mdescr="$m"
	else
	    mdescr="$m (base: $method)"
	fi
        echo "Checking GenBank loader $mdescr:"
        GENBANK_LOADER_METHOD="$m"
        export GENBANK_LOADER_METHOD
        $CHECK_EXEC "$@" $ALL_READERS_EXTRA_ARGUMENTS
        error=$?
        if test $error -ne 0; then
            echo "Test of GenBank loader $mdescr failed: $error"
            exitcode=$error
            failed="$failed $mdescr"
            case $error in
            # signal 1 (HUP), 2 (INTR), 9 (KILL), or 15 (TERM).
                129|130|137|143) echo "Apparently killed"; break ;;
            esac
        fi
        if test "$cache" = 3; then
	    remove_cache
	fi
    done
done
remove_cache

if test $exitcode -ne 0; then
    echo "Failed tests:$failed"
fi
exit $exitcode
