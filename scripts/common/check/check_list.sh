#! /bin/bash

# $Id$
# Author:  Vladimir Ivanov, NCBI 
#
###########################################################################
#
# Obtaining a check list for NCBI C++ Toolkit tree
#
# Usage:
#    check_list.sh <target_dir> <tmp_dir> <date_time> <svn_tree>
#
#    target_dir   - directory to copy list of tests -- absolute path
#                   (default is current).
#    tmp_dir      - base name of the temporary directory for svn checkout
#                   (default is current).
#    date_time    - get check list on specified date/time
#                   (default is current).
#    svn_tree     - --development or --production or --local.
#                   --local means that C++ tree is already here in
#                     the directory with name './cxx'.
#
#    If any parameter is skipped that will be used default value for it.
#
###########################################################################

# Parameters

target_dir=${1:-`pwd`}
tmp_dir=${2:-`pwd`}
date_time=${3:+"--date=$3"}
svn_tree=${4:-'--development'}

svn_core='/am/ncbiapdata/scripts/common/svn_core.sh'
src_dir="$tmp_dir/c++.checklist"
conf_name="TEST_CONF"

# Get C++ tree from SVN for Unix platform
rm -rf "$src_dir" > /dev/null
if [ "$svn_tree" == "--local" ]; then
  cp -r ./cxx "$src_dir" ||  exit 1
else
  flags="--without-gui --unix"
  if [ -n "$date_time" ]; then
    $svn_core "$src_dir" $flags "$date_time" $svn_tree ||  exit 1
  else
    $svn_core "$src_dir" $flags $svn_tree ||  exit 1
  fi
fi

# Make any configururation
cd "$src_dir"
./configure --without-internal --with-build-root=$conf_name  ||  exit 2

# Make check list
cd $conf_name/build  ||  exit 3
make check_r RUN_CHECK=N CHECK_USE_IGNORE_LIST=N

# Copy check list to target dir
cp check.sh.list "$target_dir"  ||  exit 4

# Cleanup
cd ../../..
rm -rf "$src_dir" > /dev/null

exit 0
