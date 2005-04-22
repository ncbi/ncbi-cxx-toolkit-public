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

SendMailMsg() {
    if [ "x$mail_to" != "x" ]; then
        echo "Sending an email notification to $mail_to..."
        echo $2 | mail -s "$1" $mail_to    
    fi
}

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
USAGE:  $script_name workernode [--mail-to=email]
   Check if NetSchedule Worker Node is not running and run it.
        workernode        -- Full path to a worker node.
        --mail-to=email   -- e-mail adderss where messages will be sent.(optional)

ERROR:  $1.

EOF
    cd $start_dir
    exit 2
}

StartNode() {
    echo "Starting the $node_name node..."
    if [ -f ${node_name}.out ]; then 
       cat ${node_name}.out >>  ${node_name}_out.old
    fi
    echo "[`date`] ================= " > ${node_name}.out
    $node >  ${node_name}.out  2>&1 &
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
}

StopNode() {
    echo "Stopping the $node_name node..."
    $ns_control -s $host $port > /dev/null  2>&1
    sleep $service_wait
}

CopyFile() {
    if [ -f $1 ]; then
       cp -fp $1 ${backup_dir}/
       if test $? -ne 0; then
          SendMailMsg "Upgrade Error: $node_name on $host:$port" "Couldn't copy ${BIN_PATH}/$1 to ${backup_dir}/$1"
          echo "Couldn't copy ${BIN_PATH}/$1 to ${backup_dir}/$1" >& 2
          cd $start_dir
          exit 2
       fi
    fi
    cp -fp ${new_version_dir}/$1 $1.new
    if test $? -ne 0; then
       SendMailMsg "Upgrade Error: $node_name on $host:$port" "Couldn't copy ${new_version_dir}/$1 to ${BIN_PATH}/$1.new"
       echo "Couldn't copy ${new_version_dir}/$1 to ${BIN_PATH}/$1.new" >& 2
       cd $start_dir
       exit 2
    fi
    if [ -f $1 ]; then
       rm -f ${BIN_PATH}/$1 
       if test $? -ne 0; then
          SendMailMsg "Upgrade Error: $node_name on $host:$port" "Couldn't remove ${BIN_PATH}/$1"
          echo "Couldn't remove ${BIN_PATH}/$1" >& 2
          cd $start_dir
          exit 2
       fi
    fi
    mv -f ${BIN_PATH}/$1.new ${BIN_PATH}/$1 
    if test $? -ne 0; then
       SendMailMsg "Upgrade Error: $node_name on $host:$port" "Couldn't move ${BIN_PATH}/$1.new to ${BIN_PATH}/$1"
       echo "Couldn't move ${BIN_PATH}/$1.new to ${BIN_PATH}/$1" >& 2
       cd $start_dir
       exit 2
    fi
    rm -f ${new_version_dir}/$1 
}

# ---------------------------------------------------- 

test $# -gt 0  ||  Usage "Too few arguments (workernode is required)"

node_name=`basename $1`
BIN_PATH=`dirname $1`
BIN_PATH=`(cd "${BIN_PATH}" ; pwd)`
node=${BIN_PATH}/${node_name}
ini_file=${BIN_PATH}/${node_name}.ini
service_wait=10
ns_control=${script_dir}/netschedule_control
new_version_dir=${BIN_PATH}/NEW_VERSION
backup_dir=${BIN_PATH}/BACKUP

cd ${BIN_PATH}

AddRunpath $BIN_PATH

shift

for cmd_arg in "$@" ; do
  case "$cmd_arg" in
    --mail-to=*  ) mail_to="`echo $cmd_arg | sed -e 's/--mail-to=//'`" ;;

    * ) Usage "Inbalid command line argument: $cmd_arg" ;;
  esac
done

if [ ! -f $node ]; then
    Usage "Cannot find $node"
fi

if [ ! -f $ns_control ]; then
    Usage "Cannot find $ns_control"
fi

if [ ! -f $ini_file ]; then
    Usage "Cannot find $ini_file at $BIN_PATH"
fi

if [ ! -d $backup_dir ]; then
   mkdir $backup_dir
fi

if [ ! -d $new_version_dir ]; then
   mkdir $new_version_dir
fi

host=`hostname`
port=`cat $ini_file | grep control_port= | sed -e 's/control_port=//'`

if [ -f ${new_version_dir}/rdist_me ]; then
   SendMailMsg "New Version of $node_name on $host:$port" "New version of $node_name is found for $host:$port"   
   StopNode
   echo "Upgrading the $node_name node..." 
   for file in ${new_version_dir}/* ; do
      fname=`basename $file`
      if [ $fname != "rdist_me" ]; then
         CopyFile $fname
      fi
   done
   rm -f ${new_version_dir}/rdist_me
   SendMailMsg "$node_name on $host:$port has been upgraded" "New version of $node_name on $host:$port has been installed"   
fi

echo "Testing if $node_name is alive on $host:$port"


$ns_control -v $host $port > /dev/null  2>&1
if test $? -ne 0; then
    echo "Service not responding"
    StartNode    
else
    echo "Service is alive"
    cd $start_dir
    exit 0
fi

SendMail "$node_name started (pid=$node_pid) at $host:$port"
echo $node_pid > ${node_name}.pid    
cd $start_dir

