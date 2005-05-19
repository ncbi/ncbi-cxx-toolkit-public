#! /bin/sh
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
        if [ -f "$2" ]; then
            cat $2 |  mail -s "$1" $mail_to    
        else
            echo "$2" | mail -s "$1" $mail_to    
        fi
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

ports_show="["
ports_from_file=0

send_start_msg=0
add_to_started_port_list=0

StartNode() {
    echo "Testing if $node_name is alive on $host:$1"
    $ns_control -v $host $1 > /dev/null  2>&1
    if test $? -ne 0; then
        echo "Service not responding. Starting the $node_name node..."

        if [ "x$2" == "x" ]; then
            $node -control_port $1 >>  ${node_name}.out  2>&1 &
        else 
            $node -control_port $1 -conffile $2 >>  ${node_name}.out  2>&1 &
        fi
        node_pid=$!
        echo $node_pid > ${node_name}.$1.pid    

        echo "Waiting for the service to start ($service_wait seconds)..."
        sleep $service_wait
        
        $ns_control -v $host $1 > /dev/null  2>&1
        if test $? -ne 0; then
            SendMail "[PROBLEM] $node_name on $host:$1 failed to start"
            echo "Failed to start service on $host:$1"
            add_to_started_port_list=0
        else
            add_to_started_port_list=1
            send_start_msg=1
        fi
    else
        echo "Service is alive"
    fi
}

StartNodes() {
    ports_from_file=0
    ports_show="["
    send_start_msg=0

    test  -f ${node_name}.out &&  cat ${node_name}.out >>  ${node_name}_out.old
    echo "[`date`] ================= " > ${node_name}.out

    if [ -f $worker_nodes_ports ]; then
        IFS='
'
        for line in `cat ${worker_nodes_ports}`; do
            p="`echo $line | cut -f1 -d' '`"
            c="`echo $line | cut -f2 -d' '`"
            test "x$p" = "x" && continue
            ports_from_file=1
            StartNode $p $c
            test $add_to_started_port_list -eq 1 &&  ports_show="$ports_show $p"
        done
        unset IFS
        ports_show="$ports_show ]"
    fi
    if test $ports_from_file -ne 1; then
        ports_show="$port"
        StartNode $port
    fi
    test $send_start_msg -eq 1 && SendMail "$node_name started on $host:$ports_show"
}


StopNode() {
    echo "Stopping the $node_name node on $host:$1 ..."
    $ns_control -s $host $1 > /dev/null  2>&1
    test $? -eq 0 && sleep $service_wait
}

StopNodes() {
    ports_from_file=0
    ports_show="["
    if [ -f $worker_nodes_ports ]; then
        IFS='
'
        for line in `cat ${worker_nodes_ports}`; do
            p="`echo $line | cut -f1 -d' '`"
            test "x$p" = "x" && continue
            ports_show="$ports_show $p"
            StopNode $p
            ports_from_file=1
        done
        unset IFS
        ports_show="$ports_show ]"
    fi
    if test $ports_from_file -ne 1; then
        ports_show="$port"
        StopNode $port
    fi
}

CopyFile() {
    if [ -f $1 ]; then
       cp -fp $1 ${backup_dir}/
       if test $? -ne 0; then
          SendMailMsg "Upgrade Error: $node_name on $host:$ports_show" "Couldn't copy ${BIN_PATH}/$1 to ${backup_dir}/$1"
          echo "Couldn't copy ${BIN_PATH}/$1 to ${backup_dir}/$1" >& 2
          cd $start_dir
          exit 2
       fi
    fi
    cp -fp ${new_version_dir}/$1 $1.new
    if test $? -ne 0; then
       SendMailMsg "Upgrade Error: $node_name on $host:$ports_show" "Couldn't copy ${new_version_dir}/$1 to ${BIN_PATH}/$1.new"
       echo "Couldn't copy ${new_version_dir}/$1 to ${BIN_PATH}/$1.new" >& 2
       cd $start_dir
       exit 2
    fi
    if [ -f $1 ]; then
       rm -f ${BIN_PATH}/$1 
       if test $? -ne 0; then
          SendMailMsg "Upgrade Error: $node_name on $host:$ports_show" "Couldn't remove ${BIN_PATH}/$1"
          echo "Couldn't remove ${BIN_PATH}/$1" >& 2
          cd $start_dir
          exit 2
       fi
    fi
    mv -f ${BIN_PATH}/$1.new ${BIN_PATH}/$1 
    if test $? -ne 0; then
       SendMailMsg "Upgrade Error: $node_name on $host:$ports_show" "Couldn't move ${BIN_PATH}/$1.new to ${BIN_PATH}/$1"
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
worker_nodes_ports=worker_nodes.ports

cd ${BIN_PATH}

AddRunpath $BIN_PATH

shift

for cmd_arg in "$@" ; do
  case "$cmd_arg" in
    --mail-to=*  ) mail_to="`echo $cmd_arg | sed -e 's/--mail-to=//'`" ;;

    * ) Usage "Inbalid command line argument: $cmd_arg" ;;
  esac
done

test ! -f $node &&  Usage "Cannot find $node"
test ! -f $ns_control &&  Usage "Cannot find $ns_control"
test ! -f $ini_file &&  Usage "Cannot find $ini_file at $BIN_PATH"

test ! -d $backup_dir &&  mkdir $backup_dir
test ! -d $new_version_dir &&  mkdir $new_version_dir

host=`hostname`
port=`cat $ini_file | grep control_port= | sed -e 's/control_port=//'`


if [ -f ${new_version_dir}/stop_me ]; then
   touch ${new_version_dir}/donotrun_me
   StopNodes
   SendMailMsg "$node_name stopped on $host:$ports_show" "$node_name stopped on $host:$ports_show"   
   rm -f ${new_version_dir}/stop_me  
fi

test  -f ${new_version_dir}/donotrun_me && exit 0

if [ -f ${new_version_dir}/upgrade_me ]; then
    ls -latrRF > ${node_name}.ls
    touch ${new_version_dir}/donotrun_me
    StopNodes 
    SendMailMsg "New Version of $node_name on $host:$ports_show"  "${node_name}.ls"
    echo "Upgrading the $node_name node..." 
    for file in ${new_version_dir}/* ; do
        fname=`basename $file`
        test $fname != "upgrade_me" && test $fname != "donotrun_me" && CopyFile $fname
    done
    rm -f ${new_version_dir}/upgrade_me
    rm -f ${new_version_dir}/donotrun_me

   SendMailMsg "$node_name on $host:$ports_show has been upgraded" "New version of $node_name on $host:$ports_show has been installed"   
fi

StartNodes

cd $start_dir

