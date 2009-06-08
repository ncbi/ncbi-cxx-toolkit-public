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

DEFPTB_VERSION="1.8.1"
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

DEFPTB_VERSION_FILE="${TREE_ROOT}/src/build-system/ptb_version.txt"
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

#PTB_VER=`echo $DEFPTB_VERSION | sed -e s/[.]//g`
test -z "$PTB_PLATFORM" && PTB_PLATFORM=`arch`
PTB_EXTRA="$PTB_EXTRA -ide $IDE -arch \"$PTB_PLATFORM\""

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

PTB_INI="${TREE_ROOT}/src/build-system/${ptbname}.ini"
if test ! -f "$PTB_INI"; then
  echo "ERROR: $PTB_INI not found"
  exit 1
fi
test -z "$PTB_PROJECT" && PTB_PROJECT=${PTB_PROJECT_REQ}

if test ! -x "$PTB_EXE"; then
  echo "=============================================================================="
  echo Building project tree builder locally, please wait
  echo "xcodebuild -project $BUILD_TREE_ROOT/static/UtilityProjects/PTB.xcodeproj -target $ptbname -configuration ReleaseDLL"
  echo "=============================================================================="
  xcodebuild -project $BUILD_TREE_ROOT/static/UtilityProjects/PTB.xcodeproj -target $ptbname -configuration ReleaseDLL
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

echo "=============================================================================="
echo "Running CONFIGURE, please wait."
echo "$PTB_EXE $PTB_FLAGS $PTB_EXTRA -logfile ${SLN_PATH}_configuration_log.txt -conffile $PTB_INI $TREE_ROOT $PTB_PROJECT $SLN_PATH"
echo "=============================================================================="
eval $PTB_EXE $PTB_FLAGS $PTB_EXTRA -logfile ${SLN_PATH}_configuration_log.txt -conffile $PTB_INI $TREE_ROOT $PTB_PROJECT $SLN_PATH
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
  open ${SLN_PATH}_configuration_log.txt
fi

exit 0
