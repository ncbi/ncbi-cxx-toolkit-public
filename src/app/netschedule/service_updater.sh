#! /bin/sh
#
# $Id$
#
# Author: Maxim Didenko
#
#  Updates service comptnent

script_name=`basename $0`
script_dir=`dirname $0`
script_dir=`(cd "${script_dir}" ; pwd)`

UPDATE_DIR=$1

test "x$UPDATE_DIR" = "x" && UPDATE_DIR=~/service_update

test ! -d ${UPDATE_DIR} && exit 2

${script_dir}/grid_updater.sh /export/home/service/grid_nodes ${UPDATE_DIR}/grid
${script_dir}/crontab_updater.sh ${UPDATE_DIR}/crontabs

