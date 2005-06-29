#! /bin/sh
#
# $Id$
#
# Author: Maxim Didenko
#
#  Updates grid workers directory structure

script_name=`basename $0`
script_dir=`dirname $0`
script_dir=`(cd "${script_dir}" ; pwd)`


start_dir=`pwd`


check_error() {
    test $? -eq 0 && return
    echo "$@"
    exit 2
}

cp_file() {
    test ! -f $1 && return
    cp -pf $1 $2.new
    check_error cannot cp file $1 to $2.new
    if [ -f $2 ] ; then 
        rm -f $2 
        check_error cannot rm file $2
    fi
    mv $2.new $2
}

WORK_DIR=$1
UPDATE_DIR=$2

test "x$WORK_DIR" = "x" && WORK_DIR=~/grid_nodes
test "x$UPDATE_DIR" = "x" && UPDATE_DIR=~/grid_update

 
# Update control programs
for file in ${UPDATE_DIR}/CONTROL_PROGRAMS/* ; do
    test ! -f $file && continue
    oldfile=${WORK_DIR}/`basename $file`
    diff $file $oldfile >/dev/null 2>&1
    test $? -eq 0 && continue
    cp_file $file $oldfile
done

# update nodes

for dir in ${UPDATE_DIR}/* ; do
    test ! -d $dir && continue
    test "x$dir" = "x${UPDATE_DIR}/CONTROL_PROGRAMS" && continue
    test -f $dir/NEW_VERSION/upgrade_me && continue
    conf_dir="$dir/conf/"`hostname`
    if [ ! -f $conf_dir/stop_me ]; then
        rm -f $wdir/NEW_VERSION/stop_me
        rm -f $wdir/NEW_VERSION/donotrun_me
    fi

    wdir=${WORK_DIR}/`basename $dir`
    if [ ! -d $wdir ] ; then
        mkdir $wdir
        check_error cannot mkdir $wdir
        mkdir $wdir/NEW_VERSION
        check_error cannot mkdir $wdir/NEW_VERSION
        mkdir $wdir/BACKUP
        check_error cannot mkdir $wdir/BACKUP
    fi
    count=0
    for ddd in $dir/bin $conf_dir ; do
        for f in $ddd/* ; do
            test ! -f $f && continue
            fname=`basename $f`
            diff $f $wdir/$fname >/dev/null 2>&1
            test $? -eq 0 && continue
            cp_file $f $wdir/NEW_VERSION/$fname
            count=`expr $count + 1`
        done
    done

    if [ $count -ne 0 ] ; then
        test -f $dir/scripts/pre_install.sh \
            && cp_file $dir/scripts/pre_install.sh $wdir/NEW_VERSION/pre_install.sh
        test -f $dir/scripts/post_install.sh \
            && cp_file $dir/scripts/post_install.sh $wdir/NEW_VERSION/post_install.sh
        touch $wdir/NEW_VERSION/upgrade_me
    fi

done
