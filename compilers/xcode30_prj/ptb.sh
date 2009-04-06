#!/bin/sh

# $Id$
# Author:  Andrei Gourianov, NCBI (gouriano@ncbi.nlm.nih.gov)
#
# Run project_tree_builder to generate XCode project file
#
# DO NOT ATTEMPT to run this script manually
# It should be run by CONFIGURE project only
# (open a project and build or rebuild CONFIGURE target)
#

#-----------------------------------------------------------------------------
# input environment vars:
# PTB_ARCH
# PTB_FLAGS
# PTB_PATH
# SLN_PATH
# TREE_ROOT
# BUILD_TREE_ROOT
# PTB_PROJECT

#-----------------------------------------------------------------------------
# required version of PTB
#DEFPTB_VERSION="1.8.0"
DEFPTB_VERSION="notimplemented"
DEFPTB_LOCATION="/net/snowman/vol/export2/win-coremake/App/Ncbi/cppcore/ptb/"
# Xcode version
IDE="30"

ptbname="project_tree_builder"
buildptb="no"

PTB_EXTRA="-ide $IDE"
test -z "$PTB_ARCH" && PTB_ARCH=`arch`
PTB_EXTRA="$PTB_EXTRA -arch $PTB_ARCH"

if test ! "$PREBUILT_PTB_EXE" = "bootstrap"; then
  PTB_EXE="$PREBUILT_PTB_EXE"
  if test -x "$PTB_EXE"; then
    echo "Using $ptbname at $PTB_EXE"
  else
    case "`arch`" in
      ppc  ) PLATFORM="PowerMAC" ;;
      i386 ) PLATFORM="IntelMAC" ;;
    esac
    PTB_EXE="$DEFPTB_LOCATION$PLATFORM/$DEFPTB_VERSION/$ptbname"
    if test -x "$PTB_EXE"; then
      echo "Using $ptbname at $PTB_EXE"
    else
      echo "$ptbname not found at $PTB_EXE"
      echo "Will build $ptbname locally"
      buildptb="yes"
    fi
  fi
fi

ptbini="${TREE_ROOT}src/build-system/project_tree_builder.ini"
if test ! -f "$ptbini"; then
  echo "$ptbini not found"
  exit 1
fi
test -z "$PTB_PROJECT" && PTB_PROJECT=${PTB_PROJECT_REQ}

if test "$buildptb" = "yes"; then
  echo "======================================================================"
  echo "Building project_tree_builder."
  echo "xcodebuild -project $BUILD_TREE_ROOT/static/UtilityProjects/PTB.xcodeproj -target $ptbname -configuration ReleaseDLL"
  echo "======================================================================"
  xcodebuild -project $BUILD_TREE_ROOT/static/UtilityProjects/PTB.xcodeproj -target $ptbname -configuration ReleaseDLL
  test "$?" -ne 0 && exit 1
  PTB_EXE=${PTB_PATH}/$ptbname
fi

if test ! -x "$PTB_EXE"; then
  echo "ERROR: $ptbname not found at $PTB_EXE"
  exit 1
fi
$PTB_EXE -version
if test $? -ne 0; then
  echo "ERROR: cannot find working $PTB_EXE"
  exit 1
fi

echo "======================================================================"
echo "Running CONFIGURE."
echo "$PTB_EXE $PTB_FLAGS $PTB_EXTRA -logfile ${SLN_PATH}_configuration_log.txt -conffile $ptbini $TREE_ROOT $PTB_PROJECT $SLN_PATH"
echo "======================================================================"
$PTB_EXE $PTB_FLAGS $PTB_EXTRA -logfile ${SLN_PATH}_configuration_log.txt -conffile $ptbini $TREE_ROOT $PTB_PROJECT $SLN_PATH
test "$?" -ne 0 && exit 1

if test "$TERM" = "dumb"; then
  open ${SLN_PATH}_configuration_log.txt
fi

exit 0
