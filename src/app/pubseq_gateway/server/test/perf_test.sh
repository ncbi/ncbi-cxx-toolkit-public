#! /bin/sh
#$Id$
#set -xe
src_services=$@

# You can add or remove variants in the following two lists in your local
# version of this script to reduce or to extend the test sets
builds="T"
#builds="S T"
#builds="P T"
#builds="P S T"
#builds="O P S T"
projects="objtools/test/objmgr app/objmgr/test app/pubseq_gateway/client"


# Input: host:port[/suffix]
# Output:
#   service=host:port
#   srv=host_port[_suffix]
#   ver=version[suffix]
#   parsed_services collects all services as host:port,host_port[_suffix],version[suffix]
parsed_services=
parse_service() {
    src_srv=$1
    sfx=`echo $src_srv | sed 's/^.*\///'`
    if [ "$sfx" = "$src_srv" ]; then
        sfx=
    fi
    service=`echo $src_srv | sed 's/\/.*$//'`
    ver=`curl -s "http://$service/ADMIN/info" | sed '{ s/^.*Version\"/\"/ ; s/\,.*// ; s/[^0-9]*//g }'`
    if [ "$ver" = "" ]; then
        echo "Warning: failed to get server info"
        ver="000"
    fi
    parsed_services="$parsed_services $service,$ver$sfx"
}

# Input: single service info from parse_service() output
# Output:
#   service=host:port
#   srv=host_port[_suffix]
#   ver=version[suffix]
split_parsed_service() {
    service=`echo $1 | sed 's/,.*$//'`
    ver=`echo $1 | sed 's/^.*,//'`
    srv=`echo $1 | sed 's/[\:,]/_/g'`
    echo "$1 = $service - $srv - $ver"
}

check_duplicate_services() {
    src=$@
    dst=
    for i in $src; do
        split_parsed_service $i
        s1=$service
        v1=$ver
        for j in $dst; do
            split_parsed_service $j
            s2=$service
            v2=$ver
            if [ "$v1" = "$v2" ]; then
                echo "Error: services $s1 and $s2 both have version $v1 and the same suffix"
                exit 1
            fi
        done
        dst="$dst $i"
    done
}

do_build() {
    build=$1
    proj=$2
    echo "Building $build $proj..."
    yes | import_project -noswitch $proj $NCBI/c++.$build/ReleaseDLL64MT
    error=$?
    if [ $error -ne 0 ]; then
        echo "Error: import_project $proj failed, error $error"
        return $error
    fi
    cd trunk/c++/src/$proj
    make all_r -j 15
    error=$?
    if [ $error -ne 0 ]; then
        echo "Error: build failed, error $error"
        return $error
    fi
    return 0
}

do_check() {
    bld=$1
    case $bld in
    "O") build="prev-production";;
    "P") build="production";;
    "S") build="trial";;
    "T") build="metastable";;
    esac
    for proj in $projects; do
        do_build $build $proj
        error=$?
        if [ $error -ne 0 ]; then
            cd $res_dir
            return
        fi
        for s in $parsed_services; do
            split_parsed_service $s
            echo "Testing $build / $service..."
            export PSG_LOADER_SERVICE_NAME="$service"
            export NCBI_PSG_SERVICE="$service"
            if echo "$service" | grep '^[a-zA-Z0-9_-][a-zA-Z0-9._-]*:[0-9][0-9]*$' ; then
                export PSG2_CONN_LOCAL_ENABLE=1
                export PSG2_CONN_LOCAL_SERVER_1="STANDALONE $service S=yes"
                unset PSG2_CONN_SERVICE_NAME
            else
                unset PSG2_CONN_LOCAL_ENABLE
                unset PSG2_CONN_LOCAL_SERVER_1
                export PSG2_CONN_SERVICE_NAME="$service"
            fi
            rm -f check.sh.log
            yes | make check_r
            cat check.sh.log >> $res_dir/perf_view.$bld$srv
            grep '^ERR\|^TO' check.sh.log >& /dev/null
            if [ $? -eq 0 ]; then
                out_dir=$res_dir/test_out_$bld$srv
                mkdir $out_dir
                mv *.test_out* $out_dir
            fi
        done
        cd $res_dir
        rm -rf ./trunk
    done
}

run_perf_view() {
    out=$1
    shift
    args=$@
    echo "Building table for $args..."
    trunk/c++/src/check/perf_view/perf_view $args > perf_view_out.$out
    error=$?
    if [ $error -ne 0 ]; then
        echo "Error: perf_view failed, error $error"
    fi
}

if [ "$src_services" = "" ]; then
    echo Missing service argument.
    echo Usage:
    echo "$0 <service-1:port-1>[/suffix-1] [<service-2:port-2>[/suffix-2] ...]"
    exit 0
fi
for s in $src_services; do
    x=`echo $s | grep :`
    if [ $? -ne 0 ]; then
        echo "Invalid service format: '$s'"
        echo "Expected: <service>:<port>[/suffix]"
        exit 1
    fi
    parse_service $s
done
check_duplicate_services $parsed_services

export GENBANK_LOADER_PSG=t
base_dir=`pwd`
d=`date +%Y%m%d-%H%M%S`
res_dir=$base_dir/results.$d
mkdir $res_dir
error=$?
if [ $error -ne 0 ]; then
    echo "Error: can not create output directory $res_dir"
    exit $error
fi
cd $res_dir

all_tags=
all_builds=

for bld in $builds; do
    do_check $bld
    all_builds="$all_builds $bld"
    all_services=
    for s in $parsed_services; do
        split_parsed_service $s
        echo "Service $service version=$ver"
        tag=$bld$ver
        mv $res_dir/perf_view.$bld$srv $res_dir/perf_view.$tag
        out_dir=$res_dir/test_out_$bld$srv
        if test -d $out_dir; then
            mv $out_dir $res_dir/$tag
            echo Errors detected, test output saved to $res_dir/$tag
        fi
        all_tags="$all_tags $tag"
        all_services="$all_services $ver"
    done
done

do_build metastable check/perf_view
error=$?
if [ $error -ne 0 ]; then
    exit $error
fi
cd $res_dir

run_perf_view all $all_tags
for s in $all_services; do
    perf_view_args=
    for b in $all_builds; do
        perf_view_args="$perf_view_args $b$s"
    done
    run_perf_view $s $perf_view_args
done
for b in $all_builds; do
    perf_view_args=
    for s in $all_services; do
        perf_view_args="$perf_view_args $b$s"
    done
    run_perf_view $b $perf_view_args
done

rm -rf ./trunk *.tmp
