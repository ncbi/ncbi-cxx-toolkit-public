#!/bin/sh
#
# $Id$
#
# Author: Maxim Didenko, Anatoliy Kuznetsov
#
#  Check if worker node is not running and run it

script_name=`basename $0`
script_dir=`dirname $0`
script_dir=`(cd "${script_dir}" ; pwd)`


start_dir=`pwd`


SendMail() {
    if [ "x$mail_to" != "x" ]; then
        echo "Sending an email notification to $mail_to..."
        cat ${node_name}.out | mail -s "$@" $mail_to    
    fi
}

AddRunpath()
{
    x_add_rpath="$1"

    test -z "$x_add_rpath"  &&  return 0

    if test -n "$LD_LIBRARY_PATH" ; then
        LD_LIBRARY_PATH="$x_add_rpath:$LD_LIBRARY_PATH"
    else
        LD_LIBRARY_PATH="$x_add_rpath"
    fi
    export LD_LIBRARY_PATH

    case "`uname`" in
     Darwin*)
        DYLD_LIBRARY_PATH="$LD_LIBRARY_PATH"
        export DYLD_LIBRARY_PATH
        ;;
    esac    
}

Usage() {
   cat <<EOF
USAGE:  $script_name <rundir> <workernode> <mailto>
   Check if NetSchedule Worker Node is not running and run it.
        rundir      Path where this script should be started from.
                    (ini file must be in this directory)
        workernode  Full path to a worker node.
        mailto      e-mail adderss where messages will be sent.(optional)

ERROR:  $1.

EOF
    cd $start_dir
    exit 2
}

# ---------------------------------------------------- 

test $# -ge 2  ||  Usage "Invalid number of arguments"

run_dir=$1
node=$2
mail_to=$3
node_name=`basename $2`
ini_file=${node_name}.ini
BIN_PATH=`dirname $2`
ns_control=${BIN_PATH}/netschedule_control
service_wait=15

AddRunpath $BIN_PATH

if [ ! -d "$run_dir" ]; then
    Usage "Startup directory ( $run_dir ) does not exist"
fi

cd $run_dir

if [ ! -f $node ]; then
    Usage "Cannot find $node"
fi

if [ ! -f $ini_file ]; then
    Usage "Cannot find $ini_file at  $run_dir"
fi

host=`hostname`
port=`cat $ini_file | grep control_port= | sed -e 's/control_port=//'`

echo "Testing if $node_name is alive on $host:$port"


$ns_control -v $host $port > /dev/null  2>&1
if test $? -ne 0; then
    echo "Service not responding"
    
    echo "Starting the $node_name node..."
    cat ${node_name}.out >> ${node_name}_out.old
    echo "[`date`] ================= " > ${node_name}.out
    $node >> ${node_name}.out  2>&1 &
    node_pid=$!
    echo "Waiting for the service to start ($service_wait seconds)..."
    sleep $service_wait
    
    $ns_control -v $host $port > /dev/null  2>&1
    if test $? -ne 0; then
        SendMail "[PROBLEM] $node_name @ $host:$port failed to start"
        echo "Failed to start service" >& 2
        cd $start_dir
        exit 1
    fi
else
    echo "Service is alive"
    cd $start_dir
    exit 0
fi

SendMail "$node_name started (pid=$node_pid) at $host:$port"
echo $node_pid > ${node_name}.pid    
cd $start_dir

