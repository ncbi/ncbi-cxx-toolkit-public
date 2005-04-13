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
USAGE:  $script_name workernode [mailto]
   Check if NetSchedule Worker Node is not running and run it.
        workernode  Full path to a worker node.
        mailto      e-mail adderss where messages will be sent.(optional)

ERROR:  $1.

EOF
    cd $start_dir
    exit 2
}

# ---------------------------------------------------- 

test $# -ge 1  ||  Usage "Invalid number of arguments"

node_name=`basename $1`
BIN_PATH=`dirname $1`
BIN_PATH=`(cd "${BIN_PATH}" ; pwd)`
node=${BIN_PATH}/${node_name}
ini_file=${BIN_PATH}/${node_name}.ini
mail_to=$2
service_wait=15
ns_control=${BIN_PATH}/netschedule_control

cd ${BIN_PATH}

AddRunpath $BIN_PATH

cd $run_dir

if [ ! -f $node ]; then
    Usage "Cannot find $node"
fi

if [ ! -f $ns_control ]; then
    Usage "Cannot find $ns_control"
fi

if [ ! -f $ini_file ]; then
    Usage "Cannot find $ini_file at $BIN_PATH"
fi

host=`hostname`
port=`cat $ini_file | grep control_port= | sed -e 's/control_port=//'`

echo "Testing if $node_name is alive on $host:$port"


$ns_control -v $host $port > /dev/null  2>&1
if test $? -ne 0; then
    echo "Service not responding"
    
    echo "Starting the $node_name node..."
    cat ${node_name}.out > ${node_name}_out.old
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

