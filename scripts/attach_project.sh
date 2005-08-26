#!/bin/sh

# $Id$
# Author:  Andrei Gourianov, NCBI (gouriano@ncbi.nlm.nih.gov)

#-----------------------------------------------------------------------------
#set -xv
#set -x

#-----------------------------------------------------------------------------
# defaults

initial_dir=`pwd`
script_name=`basename $0`
script_dir=`dirname $0`
script_dir=`(cd "${script_dir}" ; pwd)`

# "yes" or "no"
do_update="no"
#-----------------------------------------------------------------------------

DYLD_BIND_AT_LAUNCH=1
export DYLD_BIND_AT_LAUNCH
. ${script_dir}/common.sh

# has one optional argument: message
Usage()
{
    cat <<EOF 1>&2
USAGE: $script_name proj [-d]
SYNOPSIS:
 Replace project placeholder folders by real ones
ARGUMENTS:
  proj -- mandatory name of proj.attach file in scripts/projects or
          scripts/internal/projects folder
  -d   -- do update (otherwise just report)
EOF
    test -z "$1"  ||  echo ERROR: $1 1>&2
    exit 1
}

#-----------------------------------------------------------------------------
# analyze script arguments
proj=
if test $# -eq 0; then
  Usage "No argument provided"
fi
for cmd_arg in "$@"; do
  case "$cmd_arg" in
    -d ) do_update="yes" ;;
    *  ) proj=$cmd_arg ;;
  esac
done

#-----------------------------------------------------------------------------
# check existence of files and folders
cd $initial_dir
proj_file="$script_dir/projects/$proj.attach"
if test ! -f $proj_file; then
  proj_file="$script_dir/internal/projects/$proj.attach"
fi
test -f $proj_file || Usage "$proj.attach file not found"
#-----------------------------------------------------------------------------

while read dir cvs_loc
do
  cd $initial_dir
  if test ! -d $dir; then
    echo "ERROR: $dir is not found -- skipping"
    continue
  fi
  dir_name=`dirname $dir`
  base_name=`basename $dir`
  echo "=================================================================="
  if test $do_update = "yes"; then
    cd $dir_name
    echo "removing existing $dir"
    rm -r $base_name
    echo "------------------------------------------------------------------"
    echo "CVS checkout $cvs_loc into $dir"
    cvs co -d $base_name $cvs_loc > /dev/null
  else
    echo "$dir to be replaced by files from CVS in $cvs_loc"
  fi
done < $proj_file

echo "------------------------------------------------------------------"
echo "Done"
