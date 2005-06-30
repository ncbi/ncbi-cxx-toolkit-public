#! /bin/sh
#
# $Id$
#
# Author: Maxim Didenko
#
#  Updates crontab for the current user

script_name=`basename $0`
script_dir=`dirname $0`
script_dir=`(cd "${script_dir}" ; pwd)`

check_error() {
    test $? -eq 0 && return
    echo "$@" >& 2
    exit 2
}

UPDATE_DIR=$1

test "x$UPDATE_DIR" = "x" && UPDATE_DIR=~/crontabs

wdir=${UPDATE_DIR}/`hostname`

test ! -f ${wdir}/crontab && exit 0

crontab -l > /tmp/crontab_tmp
check_error cannot run crontab -l

diff ${wdir}/crontab /tmp/crontab_tmp >/dev/null 2>&1
test $? -eq 0 && exit 0

crontab ${wdir}/crontab
check_error cannot run crontab
crontab -l > ${wdir}/crontab
chmod g+w ${wdir}/crontab
echo "crontab on "`hostname`" has been updated" >& 2


