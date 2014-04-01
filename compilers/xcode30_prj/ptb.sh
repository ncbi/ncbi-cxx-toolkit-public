#!/bin/sh
# $Id$
# ===========================================================================
# 
#                            PUBLIC DOMAIN NOTICE
#               National Center for Biotechnology Information
# 
#  This software/database is a "United States Government Work" under the
#  terms of the United States Copyright Act.  It was written as part of
#  the author's official duties as a United States Government employee and
#  thus cannot be copyrighted.  This software/database is freely available
#  to the public for use. The National Library of Medicine and the U.S.
#  Government have not placed any restriction on its use or reproduction.
# 
#  Although all reasonable efforts have been taken to ensure the accuracy
#  and reliability of the software and data, the NLM and the U.S.
#  Government do not and cannot warrant the performance or results that
#  may be obtained by using this software or data. The NLM and the U.S.
#  Government disclaim all warranties, express or implied, including
#  warranties of performance, merchantability or fitness for any particular
#  purpose.
# 
#  Please cite the author in any work or product based on this material.
#  
# ===========================================================================
# 
# Author:  Andrei Gourianov, NCBI (gouriano@ncbi.nlm.nih.gov)
#
# Run project_tree_builder to generate XCode project file
#
# DO NOT ATTEMPT to run this script manually
# It should be run by CONFIGURE project only
# (open a project and build or rebuild CONFIGURE target)
#
# ===========================================================================

DEFPTB_LOCATION="/net/snowman/vol/export2/win-coremake/App/Ncbi/cppcore/ptb"
IDE="30"
PTB_EXTRA=""
ptbname="project_tree_builder"

for v in "$PTB_PATH" "$SLN_PATH" "$TREE_ROOT" "$BUILD_TREE_ROOT"; do
  if test "$v" = ""; then
    echo ERROR: required environment variable is missing
    echo DO NOT ATTEMPT to run this script manually
    echo It should be run by CONFIGURE project only
    exit 1
  fi
done
PTBGUI="${TREE_ROOT}/src/build-system/project_tree_builder_gui/bin/ptbgui.jar"
DEFPTB_VERSION_FILE="${TREE_ROOT}/src/build-system/ptb_version.txt"
PTB_INI="${TREE_ROOT}/src/build-system/${ptbname}.ini"
PTB_SLN="${BUILD_TREE_ROOT}/static/UtilityProjects/PTB.xcodeproj"

# -------------------------------------------------------------------------
# get PTB version: from DEFPTB_VERSION_FILE  or from PREBUILT_PTB_EXE

DEFPTB_VERSION=""
if test -r "$DEFPTB_VERSION_FILE"; then
  DEFPTB_VERSION=`cat "$DEFPTB_VERSION_FILE" | sed -e 's/ //g'`
fi
if test -x "$PREBUILT_PTB_EXE"; then
  ptbver=`$PREBUILT_PTB_EXE -version | grep ^$ptbname | sed -e s/$ptbname:// | sed -e 's/ //g'`
  if ! test "$ptbver" = "$DEFPTB_VERSION"; then
    echo "WARNING: requested PTB version $ptbver does not match default one: $DEFPTB_VERSION"
    DEFPTB_VERSION=$ptbver
  fi
fi

PTB_VER=`echo $DEFPTB_VERSION | sed -e s/[.]//g`
verno=`echo $DEFPTB_VERSION | sed -e 's/[.]/ /g'`
for v in $verno; do
  PTB_VER_MAJOR=$v
  break;
done


# -------------------------------------------------------------------------
# See if we should and can use Java GUI

REQ_GUI_CFG="no"
USE_GUI_CFG="no"
for v in $PTB_FLAGS; do
  if test "$v" = "-cfg"; then
    REQ_GUI_CFG="yes"
    break;
  fi
done
if test "$REQ_GUI_CFG" = "yes"; then
  if test $PTB_VER_MAJOR -ge 2; then 
    if test -e "$PTBGUI"; then 
      java -version >/dev/null 2>&1
      if test $? -ne 0; then
        echo "WARNING: Java not found, cannot run configuration GUI"
      else
        USE_GUI_CFG="yes"
      fi
    else
      echo WARNING: $PTBGUI not found
    fi
  fi
fi

# -------------------------------------------------------------------------
# See if we should and can use saved settings

PTB_SAVED_CFG=""
if test -n "$PTB_SAVED_CFG_REQ"; then
  if ! test -e "$PTB_SAVED_CFG_REQ"; then
      echo "ERROR: $PTB_SAVED_CFG_REQ not found"
    exit 1
  fi
  if test $PTB_VER_MAJOR -ge 2; then
    if test $PTB_VER -ge 220; then
      PTB_SAVED_CFG="-args $PTB_SAVED_CFG_REQ"
# PTB will read PTB_PROJECT from the saved settings
      PTB_PROJECT_REQ="\"\""
    fi
  fi
fi

# -------------------------------------------------------------------------
# Identify PTB_EXE

if test "$PREBUILT_PTB_EXE" = "bootstrap"; then
  DEF_PTB="$PTB_PATH/$ptbname"
else
  if test -n "$PREBUILT_PTB_EXE"; then
    if test -x "$PREBUILT_PTB_EXE"; then
      DEF_PTB="$PREBUILT_PTB_EXE"
    else
      echo ERROR: $PREBUILT_PTB_EXE not found
      exit 1
    fi
  else
    DEF_PTB="$DEFPTB_LOCATION/Xcode/$DEFPTB_VERSION/$ptbname"
  fi
fi

if test -x "$DEF_PTB"; then
  PTB_EXE="$DEF_PTB"
else
  echo "$ptbname not found at $DEF_PTB"
  PTB_EXE="$PTB_PATH/$ptbname"
fi

# -------------------------------------------------------------------------
# Misc settings

test -z "$PTB_PLATFORM" && PTB_PLATFORM=`arch`
PTB_EXTRA="$PTB_EXTRA -ide $IDE -arch \"$PTB_PLATFORM\""

if test ! -f "$PTB_INI"; then
  echo "ERROR: $PTB_INI not found"
  exit 1
fi
test -z "$PTB_PROJECT" && PTB_PROJECT=${PTB_PROJECT_REQ}

# -------------------------------------------------------------------------
# Build PTB_EXE if needed

if test ! -x "$PTB_EXE"; then
  echo "=============================================================================="
  echo Building project tree builder locally, please wait
  cmd="xcodebuild -project $PTB_SLN -target $ptbname -configuration ReleaseDLL"
  sdk=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.8.sdk
  if [ -d "$sdk" ]; then
    cmd="$cmd -sdk $sdk"
  fi
  echo "$cmd"
  echo "=============================================================================="
  $cmd
else
  echo "=============================================================================="
  echo "Using PREBUILT $ptbname at $PTB_EXE"
  echo "=============================================================================="
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

# -------------------------------------------------------------------------
# Run PTB_EXE

export __PTB__MAKE__CANDIDATE__=1
echo "=============================================================================="
echo "Running CONFIGURE, please wait."
echo "$PTB_EXE $PTB_FLAGS $PTB_EXTRA $PTB_SAVED_CFG -logfile ${SLN_PATH}_configuration_log.txt -conffile $PTB_INI $TREE_ROOT $PTB_PROJECT $SLN_PATH"
echo "=============================================================================="
if test "$USE_GUI_CFG" = "yes"; then
  eval java -jar $PTBGUI $PTB_EXE -i $PTB_FLAGS $PTB_EXTRA $PTB_SAVED_CFG -logfile ${SLN_PATH}_configuration_log.txt -conffile $PTB_INI $TREE_ROOT $PTB_PROJECT $SLN_PATH
else
  eval $PTB_EXE $PTB_FLAGS $PTB_EXTRA $PTB_SAVED_CFG -logfile ${SLN_PATH}_configuration_log.txt -conffile $PTB_INI $TREE_ROOT $PTB_PROJECT $SLN_PATH
fi
if test "$?" -ne 0; then
  PTB_RESULT=1
else
  PTB_RESULT=0
fi

if test "$PTB_RESULT" -ne 0; then
  echo "=============================================================================="
  echo "CONFIGURE has failed"
  echo "Configuration log was saved at ${SLN_PATH}_configuration_log.txt"
  echo "=============================================================================="
  exit 1
else
  echo "=============================================================================="
  echo "CONFIGURE has succeeded"
  echo "Configuration log was saved at ${SLN_PATH}_configuration_log.txt"
  echo "=============================================================================="
fi

if test "$TERM" = "dumb"; then
  if ! test "$USE_GUI_CFG" = "yes"; then
    if test -f "${SLN_PATH}_configuration_log.txt"; then
      open ${SLN_PATH}_configuration_log.txt
    fi
  fi
fi

CANDIDATE=".candidate"
curdir=`pwd`
slndir=`dirname $SLN_PATH`
cd $slndir
slndir=`pwd`
for p in *.xcodeproj; do
  if test -f "$p/project.pbxproj$CANDIDATE"; then
    cd "$p"
    for item in *; do
      if test "$item" != "project.pbxproj$CANDIDATE" -a "$item" != "project.pbxproj"; then
        rm -rf "$item"
      fi
    done
    mv -f "project.pbxproj$CANDIDATE" "project.pbxproj"
    cd $slndir
  fi
done
cd $curdir
exit 0
