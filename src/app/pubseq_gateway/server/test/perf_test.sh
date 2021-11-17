#! /bin/sh
#$Id$

services=$@
builds="P S T"
projects="objtools/test/objmgr app/objmgr/test app/pubseq_gateway/client"

if [ "$services" = "" ]; then
    echo Missing service argument.
    echo Usage:
    echo "$0 <service-1:port-1>[/suffix-1] [<service-2:port-2>[suffix-2] ...]"
    exit 0
fi
for srv in $services; do
    x=`echo $srv | grep :`
    if test $? -ne 0; then
        echo "Invalid service format: '$srv'"
        echo "Expected: <service>:<port>[/suffix]"
        exit 1
    fi
done

export GENBANK_LOADER_PSG=t
base_dir=`pwd`

get_suffix() {
    srv=`echo $1 | sed 's/[\:\/]/_/g'`
    sfx=`echo $1 | sed 's/^.*\///'`
    if [ "$sfx" = "$1" ]; then
        sfx=
    fi
    service=`echo $1 | sed 's/\/.*$//'`
}

do_build() {
    build=$1
    proj=$2
    echo "Building $build $proj..."
    echo "\n\n\n\n\n\n\n" | import_project -noswitch $proj $NCBI/c++.$build/ReleaseDLL64MT
    error=$?
    if test $error -ne 0; then
        echo "Error: import_project $proj failed, error $error"
        return $error
    fi
    cd trunk/c++/src/$proj
    make all_r -j 15
    error=$?
    if test $error -ne 0; then
        echo "Error: build failed, error $error"
        return $error
    fi
    return 0
}

do_check() {
    bld=$1
    case $bld in
    "P") build="production";;
    "S") build="trial-25";;
    "T") build="metastable";;
    esac
    for proj in $projects; do
        do_build $build $proj
        error=$?
        if test $error -ne 0; then
            return
        fi
        for s in $services; do
            get_suffix $s
            echo "Testing $build / $service..."
            export PSG_LOADER_SERVICE_NAME=$service
            rm -f check.sh.log
            echo "\n\n" | make check_r
            log_file=`echo $proj | sed 's/\//_/g'`
            fname=$base_dir/$bld.$srv.$log_file.tmp
            mv check.sh.log $fname
            cat $fname >> $base_dir/perf_view.$bld$srv
        done
        cd $base_dir
        rm -rf ./trunk
    done
}

rm -rf ./trunk ./perf_view.* ./*.tmp
all_tags=

for bld in $builds; do
    do_check $bld
    for s in $services; do
        get_suffix $s
        ver=`curl -s "http://$service/ADMIN/info" | sed '{ s/^.*Version// ; s/\,.*// ; s/[^0-9]*//g }'`
        if [ "$ver" = "" ]; then
            echo "Warning: failed to get server info"
            ver="000"
        fi
        echo "Service $service version=$ver"
        tag=$bld$ver$sfx
        mv $base_dir/perf_view.$bld$srv $base_dir/perf_view.$tag
        all_tags="$all_tags $tag"
    done
done

do_build metastable check/perf_view
error=$?
if test $error -ne 0; then
    exit $error
fi
cd $base_dir
echo "Running 'perf_view $all_tags'..."
trunk/c++/src/check/perf_view/perf_view $all_tags > perf_view.out
error=$?
if test $error -ne 0; then
    echo "Error: perf_view failed, error $error"
    exit $error
fi
rm -rf ./trunk *.tmp
