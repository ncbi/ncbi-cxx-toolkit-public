#! /bin/sh
#$Id$

status_dir="$CFG_LIB/../status"
if test ! -d "$status_dir"; then
    status_dir="$CFG_LIB/../../status"
fi
if test ! -d "$status_dir"; then
    status_dir="../../../../status"
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
netcache=""

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
    if disabled in-house-resources; then
        echo "Skipping PUBSEQOS and ID1 loader test (in-house resources unavailable)"
        methods="$methods"
    elif disabled PubSeqOS; then
        echo "Skipping PUBSEQOS loader test (loader unavailable)"
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

clear_cache() {
    if test -n "$netcache"; then
	echo "Clearing netcache at $netcache"
        for c in $ncs; do
            echo "Clearing netcache $netcache/$c"
            if grid_cli --admin purge --nc "$netcache" --auth netcache_admin $c; then
                :
            else
		return 1;
            fi
        done
	return 0
    else
	if test -d "$cache_dir"; then
	    echo "Init BDB cache in $cache_dir"
	    rm -rf "$cache_dir"
	fi
	return 0
    fi
}

cache_disk_is_slow() {
    # disable disk cache on Windows
    if disabled MSWin; then
	:
    else
	echo Running on MSWin
	return 0
    fi
    # disable disk cache on Cygwin
    if disabled Cygwin; then
	:
    else
	echo Running on Cygwin
	return 0
    fi
    # disable disk cache on BSD
    if disabled BSD; then
	:
    else
	echo Running on BSD
	return 0
    fi
    # disable disk cache on CentOS 7
    if grep -q 'CPE_NAME="cpe:/o:centos:centos:7"' /etc/os-release 2>/dev/null; then
	echo Running on CentOS 7
	return 0
    fi
    return 1
}

test_name=`basename "$1"`

init_cache_config() {
    if test "$no_cache" = 1; then
	# already disabled
	return 1
    fi
    if disabled MT; then
        echo MT is not enabled, cache is disabled
	no_cache=1
        return 1
    fi
    if disabled DLL; then
        echo No DLL plugins, cache is disabled
	no_cache=1
        return 1
    fi
    # netcache port on localhost
    case "$NCBI_CONFIG_OVERRIDES" in
	*.netcache)
	    f=~/"$NCBI_CONFIG_OVERRIDES"
	    netcache="localhost:9135"
	    ncs="ids blobs"
	    if test -r "$f"; then
		h=`grep '^host *=' $f | sed 's/.*= *//'`
		p=`grep '^port *=' $f | sed 's/.*= *//'`
		if  test -n "$h" && test -n "$p"; then
		    netcache="$h:$p"
		fi
		s=`grep '^service *=' $f | sed 's/.*= *//'`
		if  test -n "$s"; then
		    netcache="$s"
		fi
		a=`grep '^name *= *ids' $f | sed 's/.*= *//'`
		b=`grep '^name *= *blobs' $f | sed 's/.*= *//'`
		if  test -z "$a"; then
		    a=ids
		fi
		if  test -z "$b"; then
		    b=blobs
		fi
		nes="$a $b"
	    fi
	    ;;
	*)
	    netcache="";;
    esac
    if test -n "$netcache"; then
	# check netcache
	if clear_cache; then
	    :
	else
	    echo Failed to clear netcache, cache is disabled
	    unset NCBI_CONFIG_OVERRIDES
	    netcache=""
	    no_cache=1
	    return 1
	fi
    else
	# check BDB cache
	if disabled BerkeleyDB; then
            echo BerkeleyDB is not enabled, cache is disabled
	    no_cache=1
            return 1
	fi
	if cache_disk_is_slow; then
	    echo Disk for cache is slow, cache is disabled
	    no_cache=1
	    return 1
	fi
	base_cache_dir="./$test_name.$$.genbank_cache"
	unset cache_dir
    fi
    return 0
}

init_cache() {
    if test -n "$netcache"; then
	:
    else
	cache_dir="$base_cache_dir"
	NCBI_CONFIG__GENBANK_SLASH_CACHE_SLASH_BLOB_CACHE_SLASH_BDB__PATH="$cache_dir"
	NCBI_CONFIG__GENBANK_SLASH_CACHE_SLASH_ID_CACHE_SLASH_BDB__PATH="$cache_dir"
	export NCBI_CONFIG__GENBANK_SLASH_CACHE_SLASH_BLOB_CACHE_SLASH_BDB__PATH
	export NCBI_CONFIG__GENBANK_SLASH_CACHE_SLASH_ID_CACHE_SLASH_BDB__PATH
	trap "rm -rf \"$cache_dir\"" 0 1 2 15
    fi
    clear_cache
}

init_cache_config

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
	    clear_cache
	fi
    done
done
clear_cache

if test $exitcode -ne 0; then
    echo "Failed tests:$failed"
fi
exit $exitcode
