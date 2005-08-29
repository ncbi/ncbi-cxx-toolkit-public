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
do_update="yes"

#-----------------------------------------------------------------------------

. ${script_dir}/common.sh

# has one optional argument: message
Usage()
{
    cat <<EOF 1>&2
USAGE: $script_name proj [-n]
SYNOPSIS:
 Replace project placeholder folders by real ones
ARGUMENTS:
  proj -- mandatory name of proj.attach file in scripts/projects or
          scripts/internal/projects folder
  -n   -- do not actually do anything, just report
EOF
    test -z "$1"  ||  echo ERROR: $1 1>&2
    exit 1
}


#-----------------------------------------------------------------------------
# analyze script arguments

proj=
if test $# -eq 0 ; then
  Usage "No argument provided"
fi
for cmd_arg in "$@"; do
  case "$cmd_arg" in
    -n ) do_update="no" ;;
    *  ) if -n "$proj"
           Usage "Only one project file is allowed"
         else
           proj="$cmd_arg"
         fi
         ;;
  esac
done



#-----------------------------------------------------------------------------
# check existence of files and folders

proj_file="$proj"
if test ! -f "$proj_file" ; then
  proj_file="$script_dir/projects/$proj.attach"
  if test ! -f $proj_file ; then
    proj_file="$script_dir/internal/projects/$proj.attach"
  fi
fi
test -f $proj_file  ||  Usage "Project file '$proj.attach' not found"


#-----------------------------------------------------------------------------

cat "$proj_file" | while read dir cvs_loc
do
  cd "$initial_dir"
  if test ! -d "$dir" ; then
    COMMON_Error "$dir is not found"
  fi
  dir_name=`dirname $dir`
  base_name=`basename $dir`
  echo "=================================================================="
  if test "$do_update" = "yes" ; then
    COMMON_Exec cd $dir_name
    echo "removing existing $dir"
    COMMON_Exec rm -r "$base_name"
    echo "------------------------------------------------------------------"
    echo "CVS checkout $cvs_loc into $dir"
    COMMON_Exec cvs co -d "$base_name" "$cvs_loc" > /dev/null
  else
    echo "$dir to be replaced by files from CVS in $cvs_loc"
  fi
done

echo "------------------------------------------------------------------"
echo "Done"
