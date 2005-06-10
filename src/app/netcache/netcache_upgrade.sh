#!/bin/sh
#
# $Id$
#
# Author: Anatoliy Kuznetsov
#
#  Netcache upgrade script
#
# How to run:
#   netcache_upgrade.sh base_dir_name update_dir_name {link1 link2}
#
# Script takes 
#


Die() {
    echo "$@" >& 2
    cd $start_dir
    exit 1
}

Success() {
    echo "$@" >& 2
    cd $start_dir
    exit 0
}

# ---------------------------------------------------- 


current_date=`date '+%Y%m%d'`

start_dir=`pwd`

base_dir="$1"
if [ ! -d $base_dir ]; then
    Die "Cannot find base directory for the upgrade: $base_dir" 
fi

update_dir="$2"
if [ ! -d $update_dir ]; then
    Die "Cannot find update directory for the upgrade: $update_dir" 
fi


link1="$3"
link2="$4"


base_dir_name=`dirname $base_dir`

target_dir=$base_dir_name/$current_date

if [ ! -d $target_dir ]; then
    mkdir $target_dir || Die "Cannot create target directory: $target_dir"
    chmod g+wrx $target_dir
fi

cp -R $base_dir/* $target_dir/. || Die "Cannot copy base directory to target"

if [ -f $update_dir/netcached ]; then
   echo Copying netcached to $target_dir
   cp $update_dir/netcached $target_dir/ 
fi
cp $update_dir/*.so $target_dir/

cd $base_dir_name
lpath=$link1
if [ "x$lpath" != "x" ]; then
    if [ -d $lpath ]; then
        echo "Link $lpath reassigned"
        rm $lpath
        ln -s $target_dir $lpath
    else 
        ln -s $target_dir $lpath
    fi
fi

lpath=$link2
if [ "x$lpath" != "x" ]; then
    if [ -d $lpath ]; then
        echo "Link $lpath reassigned"
        rm $lpath
        ln -s $target_dir $lpath
    else
        ln -s $target_dir $lpath
    fi
fi

Success "NetCache upgrade done."
