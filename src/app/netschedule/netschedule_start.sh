#!/bin/sh
#
# $Id$
#
# Author: Anatoliy Kuznetsov
#
#  Check if netscheduled is not running and run it

start_dir=`pwd`

Die() {
    echo "$@" >& 2
    cd $start_dir
    exit 1
}

Success() {
    echo "$@"
    echo $ns_pid > netscheduled.pid    
    cat netscheduled.out | mail -s "$@" $mail_to    
    cd $start_dir
    exit 0
}

# ---------------------------------------------------- 


run_dir=$1
ini_file=netscheduled.ini
BIN_PATH=~/cxx/GCC340-ReleaseMT/build/app/netschedule/
ns_control=${BIN_PATH}netschedule_control
netscheduled=${BIN_PATH}netscheduled
service_wait=15
mail_to="kuznets@ncbi -c vakatov@ncbi"

LD_LIBRARY_PATH=$BIN_PATH; export LD_LIBRARY_PATH

if [ ! -d "$run_dir" ]; then
    Die "Startup directory ( $run_dir ) does not exist"
fi

cd $run_dir

if [ ! -f $ini_file ]; then
    Die "Cannot find $ini_file at  $run_dir"
fi

host=`hostname`
port=`cat netscheduled.ini | grep port= | sed -e 's/port=//'`

echo "Testing if netscheduled is alive on $host:$port"


if ! $ns_control -v $host $port > /dev/null  2>&1; then
    echo "Service not responding"
    
    echo "Starting the netscheduled service..."
    cat netscheduled.out >> netscheduled_out.old
    $netscheduled > netscheduled.out  2>&1 &
    ns_pid=$!
    echo "Waiting for the service to start ($service_wait seconds)..."
    sleep $service_wait
    
    if ! $ns_control -v $host $port > /dev/null  2>&1; then
        echo "Service failed to start in $service_wait seconds"
        
        echo "Giving it $service_wait seconds more..."
        sleep $service_wait
        
        if ! $ns_control -v $host $port > /dev/null  2>&1; then
            cat netscheduled.out | mail -s "[PROBLEM] netscheduled @ $host:$port failed to start" $mail_to
            
            kill $ns_pid
            sleep 3
            kill -9 $ns_pid
            
            echo "Database reinitialization."
            
            $netscheduled -reinit >> netscheduled.out  2>&1 &
            ns_pid=$!
            
            echo "Waiting for the service to start ($service_wait seconds)..."
            sleep $service_wait
            
            if ! $ns_control -v $host $port > /dev/null  2>&1; then
                echo "Service failed to start with database reinitialization"
                cat netscheduled.out | mail -s "[PROBLEM] netscheduled @ $host:$port failed to start with -reinit" $mail_to
                Die "Failed to start service"
            else
                Success "netscheduled started with -reinit (pid=$ns_pid) at $host:$port"
            fi                        
        fi        
    fi
else
    echo "Service is alive"
    cd $start_dir
    exit 0
fi

Success "netscheduled started (pid=$ns_pid) at $host:$port"

cd $start_dir
