#!/bin/sh

# $Id$
# Author:  Andrei Gourianov, NCBI (gouriano@ncbi.nlm.nih.gov)

#-----------------------------------------------------------------------------
#set -xv
#set -x

# defaults
solution="Makefile.flat"
logfile="Flat.configuration_log"
extptb="$NCBI/c++.frozen/Release/bin/project_tree_builder"
#-----------------------------------------------------------------------------

initial_dir=`pwd`
script_name=`basename $0`
script_dir=`dirname $0`
script_dir=`(cd "${script_dir}" ; pwd)`

. ${script_dir}/common.sh


# has one optional argument: error message
Usage()
{
    cat <<EOF 1>&2
USAGE: $script_name BuildDir [-s SrcDir] [-p ProjectList] [-b] 
SYNOPSIS:
 Create flat makefile for a given build tree.
ARGUMENTS:
  BuildDir  -- mandatory. Root dir of the build tree (eg ~/c++/GCC340-Debug)
  -s        -- optional.  Root dir of the source tree (eg ~/c++)
  -p        -- optional.  List of projects: subtree of the source tree, or LST file
  -b        -- optional.  Build project_tree_builder locally
EOF
    test -z "$1"  ||  echo ERROR: $1 1>&2
    exit 1
}


#-----------------------------------------------------------------------------
# analyze script arguments


test $# -lt 1 && Usage "Mandatory argument is missing"
COMMON_Exec cd $initial_dir
COMMON_Exec cd $1
a1=`pwd`
builddir="$a1/build"
srcdir="$a1/.."
projectlist="src"
buildptb="no"
shift

dest=""
for cmd_arg in "$@"; do
  case "$dest" in
    src  )  dest="";  srcdir="$cmd_arg"     ;  continue ;;
    prj  )  dest="";  projectlist="$cmd_arg";  continue ;;
    *    )  dest=""                                     ;;
  esac
  case "$cmd_arg" in
    -s )  dest="src" ;;
    -p )  dest="prj" ;;
    -b )  dest="";    buildptb="yes" ;;
    *  )  Usage "Invalid command line argument:  $cmd_arg"
  esac
done

test -d "$builddir"  || Usage "$builddir is not a directory"
test -d "$srcdir"    || Usage "$srcdir is not a directory"
if test ! -f "$srcdir/$projectlist"; then
  test -d "$srcdir/$projectlist" || Usage "$srcdir/$projectlist not found"
fi


#-----------------------------------------------------------------------------
# more checks
if test $buildptb = "no"; then
  ptb="$extptb"
  if test -x "$ptb"; then
    ptbver=`$ptb -version 2>&1 | sed -e 's/[a-zA-Z._: ]//g'`
    if test $ptbver -lt 140; then
      $ptb -version 2>&1
      echo "Prebuilt project_tree_builder at"
      echo $extptb
      echo "is too old. Will build PTB locally"
      buildptb="yes"
    fi
  else
    echo "Prebuilt project_tree_builder is not found at"
    echo $extptb
    echo "Will build PTB locally"
    buildptb="yes"
  fi
fi

COMMON_Exec cd $builddir
dll=""
test -f "../status/DLL.enabled" && dll="-dll"
ptbini="$srcdir/compilers/msvc710_prj/project_tree_builder.ini"
test -f "$ptbini" || Usage "$ptbini not found"


#-----------------------------------------------------------------------------
# build project_tree_builder

COMMON_Exec cd $builddir
if test "$buildptb" = "yes"; then
#  ptbdep="corelib util serial serial/datatool app/project_tree_builder"
  ptbdep="corelib util app/project_tree_builder"
  for dep in $ptbdep; do
    test -d "$dep"          || Usage "$builddir/$dep not found"
    test -f "$dep/Makefile" || Usage "$builddir/$dep/Makefile not found"
  done

  echo "**********************************************************************"
  echo "Building project_tree_builder"
  echo "**********************************************************************"
  for dep in $ptbdep; do
    COMMON_Exec cd $builddir
    COMMON_Exec cd $dep
    COMMON_Exec make
  done
  COMMON_Exec cd $builddir
  ptb="./app/project_tree_builder/project_tree_builder"
  test -x "$ptb" || Usage "$builddir/$ptb not found"
fi


#-----------------------------------------------------------------------------
# run project_tree_builder

COMMON_Exec cd $builddir
echo "**********************************************************************"
echo "Running project_tree_builder. Please wait."
echo "**********************************************************************"
echo $ptb $dll -conffile $ptbini -logfile $logfile $srcdir $projectlist $solution
COMMON_Exec $ptb $dll -conffile $ptbini -logfile $logfile $srcdir $projectlist $solution
echo "Done"
