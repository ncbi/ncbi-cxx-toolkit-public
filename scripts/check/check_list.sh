#! /bin/sh

# $Id$
# Author:  Vladimir Ivanov, NCBI 
#
###########################################################################
#
# Obtaining a check list for NCBI C++ Toolkit tree
#
# Usage:
#    check_list.sh <target_dir> <tmp_dir>
#
#    target_dir   - directory to copy list of tests -- absolute path
#                   (default is current)
#    tmp_dir      - base name of the temporary directory for cvs checkout
#                   (default is current)
#
#    If any parameter is skipped that will be used default value for it.
#
###########################################################################

# Parameters

target_dir=${1:-`pwd`}
tmp_dir=${2:-`pwd`}

cvs_core=${NCBI:-/netopt/ncbi_tools}/c++/scripts/cvs_core.sh
src_dir="$tmp_dir/c++.checklist"
conf_name="TEST_CONF"

# Get C++ tree from CVS for Unix platform
rm -rf $src_dir > /dev/null
$cvs_core $src_dir --without-gui --without-objects --unix || exit 1

# Make any configururation
cd $src_dir
./configure --without-internal --with-build-root=$conf_name  ||  exit 2

# Make check list
cd $conf_name/build  ||  exit 3
make check_r RUN_CHECK=N

# Copy check list to target dir
cp check.sh.list $target_dir  ||  exit 4

# Cleanup
cd ../../..
rm -rf $src_dir > /dev/null

exit 0
