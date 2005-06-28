#! /bin/sh
#
# $Id$
#
# Author: Maxim Didenko
#
#  Run a worker node and when it finishes rerun it again

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

# ---------------------------------------------------- 

test $# -gt 1  || exit 2

node_name=`basename $1`
BIN_PATH=`dirname $1`
BIN_PATH=`(cd "${BIN_PATH}" ; pwd)`
node=${BIN_PATH}/${node_name}
new_version_dir=${BIN_PATH}/NEW_VERSION
port="$2"
conf_file="$3"

AddRunpath $BIN_PATH

test ! -f $node &&  exit 2
test "x$port" == "x"  && exit 2

cd $BIN_PATH

while : ; do
    test -f ${new_version_dir}/upgrade_me && sleep 10 && continue
    test -f ${new_version_dir}/stop_me && break
    test -f ${new_version_dir}/donotrun_me && break
    
    if [ "x$conf" == "x" ]; then
        echo "$node -control_port $port"
        $node -control_port $port &
    else 
        echo "$node -control_port $port -conffile $conf"
        $node -control_port $port -conffile $conf &
    fi
    test $? -ne 0 && break
    node_pid=$!

    rm -f ${new_version_dir}/relaunch.${port}
    wait $node_pid
    touch ${new_version_dir}/relaunch.${port}
done

rm -f ${new_version_dir}/relaunch.${port}
cd $start_dir

