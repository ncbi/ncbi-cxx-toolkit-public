#!/bin/sh

# $Id$
# Author:  Andrei Gourianov, NCBI (gouriano@ncbi.nlm.nih.gov)

#-----------------------------------------------------------------------------
#set -xv
#set -x

# defaults
solution="Makefile.flat"
logfile="Flat.configuration_log"
relroot="/net/snowman/vol/export2/win-coremake/App/Ncbi/cppcore"

ptbname="project_tree_builder"
# release path to project_tree_builder
relptbpath="$relroot/ptb/"
# release path to datatool
reldatatoolpath="$relroot/datatool/"
# Configuration GUI
ptbgui="src/build-system/project_tree_builder_gui/bin/ptbgui.jar"

# dependencies
ptbdep="corelib util util/regexp util/xregexp build-system/project_tree_builder"
#-----------------------------------------------------------------------------

initial_dir=`pwd`
script_name=`basename $0`
script_dir=`dirname $0`
script_dir=`(cd "${script_dir}" ; pwd)`

. ${script_dir}/../common.sh


# has one optional argument: error message
Usage()
{
    cat <<EOF 1>&2
USAGE: $script_name BuildDir [-s SrcDir] [-p ProjectList] [-b] [-remoteptb] [-cfg] [-saved SavedCfg]
SYNOPSIS:
 Create flat makefile for a given build tree.
ARGUMENTS:
  BuildDir   -- mandatory. Root dir of the build tree (eg ~/c++/GCC340-Debug)
  -s         -- optional.  Root dir of the source tree (eg ~/c++)
  -p         -- optional.  List of projects: subtree of the source tree, or LST file
  -b         -- optional.  Build project_tree_builder locally
  -remoteptb -- optional.  Use prebuilt project_tree_builder only; do not attempt to build it locally
  -cfg       -- optional.  Use Configuration GUI application.
  -saved     -- optional.  Use saved configuration settings from SavedCfg file
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
remoteptbonly="no"
req_gui_cfg="no"
savedcfg=""
PLATFORM=`COMMON_DetectPlatform`
shift

dest=""
for cmd_arg in "$@"; do
  case "$dest" in
    src  )  dest="";  srcdir="$cmd_arg"     ;  continue ;;
    prj  )  dest="";  projectlist="$cmd_arg";  continue ;;
    cfg  )  dest="";  savedcfg="$cmd_arg";     continue ;;
    *    )  dest=""                                     ;;
  esac
  case "$cmd_arg" in
    -s )  dest="src" ;;
    -p )  dest="prj" ;;
    -b )  dest="";    buildptb="yes" ;;
    -remoteptb )  dest="";    remoteptbonly="yes" ;;
    -cfg       )  dest="";    req_gui_cfg="yes" ;;
    -saved     )  dest="cfg" ;;
    *  )  Usage "Invalid command line argument:  $cmd_arg"
  esac
done

test -d "$builddir"  || Usage "$builddir is not a directory"
test -d "$srcdir"    || Usage "$srcdir is not a directory"
case "$projectlist" in
  /* ) abs_projectlist=$projectlist ;;
  *  ) abs_projectlist=$srcdir/$projectlist ;;
esac
if test ! -f "$abs_projectlist"; then
  test -d "$abs_projectlist" || Usage "$abs_projectlist not found"
fi
if test -n "$savedcfg"; then
  if test ! -f "$savedcfg"; then
    if test -f "$initial_dir/$savedcfg"; then
      savedcfg="$initial_dir/$savedcfg"
    else
      Usage "$savedcfg not found"
    fi
  fi
fi

#-----------------------------------------------------------------------------
# get required version of PTB
ptbreqver=""
ptbver="$srcdir/src/build-system/ptb_version.txt"
if test -r "$ptbver"; then
  ptbreqver=`cat "$ptbver" | sed -e 's/ //'`
  if test "$ptbreqver" = 2.5.0 -a -f $a1/status/DLL_BUILD.enabled; then
    echo "Forcing use of local project_tree builder."
    buildptb=yes
  fi
fi

#-----------------------------------------------------------------------------
# find PTB
if test $buildptb = "no"; then
  if test "$PREBUILT_PTB_EXE" = "bootstrap"; then
    ptb="$builddir/build-system/project_tree_builder/$ptbname"
    if test ! -x "$ptb"; then
      echo "$ptbname is not found at $ptb"
      echo "Will build $ptbname locally"
      buildptb="yes"
    fi
  elif test -n "$PREBUILT_PTB_EXE"; then
    if test -x "$PREBUILT_PTB_EXE"; then
      ptb="$PREBUILT_PTB_EXE"
      echo "Using $ptbname at $ptb"
    else
      echo "ERROR: $PREBUILT_PTB_EXE not found"
      exit 1
    fi
  else
    ptb="$relptbpath$PLATFORM/$ptbreqver/$ptbname"
    if test -x "$ptb"; then
      echo "Using $ptbname at $ptb"
    else
      if test $remoteptbonly = "yes"; then
        echo "Prebuilt $ptbname not found"
	exit 0
      fi
      echo "$ptbname is not found at $ptb"
      echo "Will build $ptbname locally"
      buildptb="yes"
    fi
  fi
fi

COMMON_Exec cd $builddir
dll=""
test -f "../status/DLL.enabled" && dll="-dll"
ptbini="$srcdir/src/build-system/$ptbname.ini"
test -f "$ptbini" || Usage "$ptbini not found"

#-----------------------------------------------------------------------------
# build project_tree_builder

COMMON_Exec cd $builddir
if test "$buildptb" = "yes"; then
  for dep in $ptbdep; do
    if test ! -d "$dep"; then
      echo "WARNING: $builddir/$dep not found"
      buildptb="no"
      break;
    fi
    if test ! -f "$dep/Makefile"; then
      echo "WARNING: $builddir/$dep/Makefile not found"
      buildptb="no"
      break;
    fi
  done
fi

if test "$buildptb" = "yes"; then
  echo "**********************************************************************"
  echo "Building $ptbname"
  echo "**********************************************************************"
  for dep in $ptbdep; do
    COMMON_Exec cd $builddir
    COMMON_Exec cd $dep
    COMMON_Exec make
  done
  COMMON_Exec cd $builddir
  ptb="./build-system/project_tree_builder/$ptbname"
  test -x "$ptb" || Usage "$builddir/$ptb not found"
  COMMON_AddRunpath $builddir/../lib
fi

test -x "$ptb" || Usage "$ptbname not found at $ptb"

#-----------------------------------------------------------------------------
# get version of project_tree_builder

$ptb -version >/dev/null 2>&1
if test $? -ne 0; then
  echo "ERROR: $ptb does not work"
  exit 1
fi
#ptbver=`$ptb -version | grep ^$ptbname | sed -e s/$ptbname:// | sed -e 's/ //g'`
ptbver=`$ptb -version | sed -ne "s/^$ptbname: *//p"`

verno=`echo $ptbver | sed -e 's/[.]/ /g'`
for v in $verno; do
  ptb_ver_major=$v
  break
done
verno=`echo $ptbver | sed -e 's/[.]//g'`

# see if we can use GUI
use_gui_cfg="no"
if test "$req_gui_cfg" = "yes"; then
  if test $ptb_ver_major -ge 2; then
    if test -e "$srcdir/$ptbgui"; then 
      java -version >/dev/null 2>&1
      if test $? -ne 0; then
        echo "WARNING: Java not found, cannot run configuration GUI"
      else
        use_gui_cfg="yes"
      fi
    else
      echo WARNING: $srcdir/$ptbgui not found
    fi
  fi
fi

# see if we can use saved settings
ptb_saved_cfg=""
if test -n "$savedcfg"; then
  if test $ptb_ver_major -ge 2; then
    if test $verno -ge 220; then
      ptb_saved_cfg="-args $savedcfg"
# PTB will read projectlist from the saved settings
      projectlist="\"\""
    fi
  fi
fi

#-----------------------------------------------------------------------------
# find datatool
ptb251="no"
dtfound="no"
dtdep=""
if test $ptb_ver_major -ge 2 -a $verno -gt 250; then
  ptb251="yes"
  if test "$PREBUILT_DATATOOL_EXE" = "bootstrap"; then
    echo "WARNING: Using in-tree datatool"
  else
    dtreqver="."
    dtver="$srcdir/src/build-system/datatool_version.txt"
    if test -r "$dtver"; then
      dtreqver=`cat "$dtver" | sed -e 's/ //'`
    fi
    datatool="$reldatatoolpath$PLATFORM/$dtreqver/datatool"
    if test -x "$datatool"; then
      $datatool -version >/dev/null 2>&1
      if test $? -eq 0; then
        dtfound="yes"
      else
        echo "WARNING: $datatool does not work"
      fi
    fi
  fi
fi
if test "$dtfound" = "no" -a $ptb251 = "yes"; then
  dtdep="-dtdep"
fi

#-----------------------------------------------------------------------------
# run project_tree_builder

COMMON_Exec cd $builddir
echo "**********************************************************************"
echo "Running $ptbname. Please wait."
echo "**********************************************************************"
echo $ptb $dll $dtdep $ptb_saved_cfg -conffile $ptbini -logfile $logfile $srcdir $projectlist $solution
if test "$use_gui_cfg" = "yes"; then
  COMMON_Exec java -jar $srcdir/$ptbgui $ptb -i $dll $dtdep $ptb_saved_cfg -conffile $ptbini -logfile $logfile $srcdir $projectlist $solution
else
  COMMON_Exec $ptb $dll $dtdep $ptb_saved_cfg -conffile $ptbini -logfile $logfile $srcdir $projectlist $solution
fi

#-----------------------------------------------------------------------------
# generate sources
if test "$dtfound" = "yes"; then
  if test -f "$builddir/../status/objects.enabled"; then
    if test -r "$solution"; then
      echo "**********************************************************************"
      echo "Generating objects source code. Please wait."
      echo "**********************************************************************"
      echo make -f $solution all_files
      make -f $solution all_files >/dev/null 2>&1
    fi
  fi
fi

echo "Done"
