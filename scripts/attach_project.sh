#!/bin/sh

# $Id$
# Author:  Andrei Gourianov, NCBI (gouriano@ncbi.nlm.nih.gov)

#-----------------------------------------------------------------------------
#set -xv

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
  -n   -- do not change anything, just report
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
    *  ) if test -n "$proj"; then
           Usage "Only one project file is allowed"
         else
           proj="$cmd_arg"
         fi
         ;;
  esac
done

#-----------------------------------------------------------------------------
# check existence of input file

prjfile="$proj"
if test ! -f "$prjfile" ; then
  prjfile="$script_dir/projects/$proj.attach"
  if test ! -f "$prjfile" ; then
    prjfile="$script_dir/internal/projects/$proj.attach"
  fi
fi
test -f "$prjfile"  ||  Usage "Project file '$proj.attach' not found"


#-----------------------------------------------------------------------------
# check existence and CVS status of folders
cat "$prjfile" | while read src_loc cvs_loc
do
  test ! -d "$src_loc" && COMMON_Error "$src_loc is not found"
done

# replace folders
cat "$prjfile" | while read src_loc cvs_loc
do
  dir_name=`dirname $src_loc`
  base_name=`basename $src_loc`
  echo "=================================================================="
  COMMON_Exec cd "$initial_dir/$src_loc"
  files=`ls | sed -e '/CVS/d'`
  if test -n "$files"; then
    echo "WARNING: $src_loc is not empty -- SKIPPING"
    continue
  fi
  COMMON_Exec cd "$initial_dir/$dir_name"
  if test "$do_update" = "yes" ; then
    echo "Removing empty $src_loc"
    COMMON_Exec rm -r "$base_name"
    echo "------------------------------------------------------------------"
    echo "CVS checkout $cvs_loc into $src_loc"
    COMMON_Exec cvs co -d "$base_name" "$cvs_loc" > /dev/null
  else
    echo "$src_loc to be replaced by files from CVS in $cvs_loc"
  fi
done

echo "------------------------------------------------------------------"
echo "Done"
