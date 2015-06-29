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
# Run datatool.exe to generate sources from ASN/DTD/Schema specifications
#
# DO NOT ATTEMPT to run this bat file manually
#
# ===========================================================================

DEFDT_LOCATION="/net/snowman/vol/export2/win-coremake/App/Ncbi/cppcore/datatool"

for v in "$DATATOOL_PATH" "$TREE_ROOT" "$BUILD_TREE_ROOT"; do
  if test "$v" = ""; then
    echo error: required environment variable is missing
    echo DO NOT ATTEMPT to run this script manually
    exit 1
  fi
done
DEFDT_VERSION_FILE="${TREE_ROOT}/src/build-system/datatool_version.txt"
PTB_SLN="${BUILD_TREE_ROOT}/static/UtilityProjects/PTB.xcodeproj"
DT="datatool"

# -------------------------------------------------------------------------
# get DT version: from DEFDT_VERSION_FILE  or from PREBUILT_DATATOOL_EXE

DEFDT_VERSION=""
if test -r "$DEFDT_VERSION_FILE"; then
  DEFDT_VERSION=`cat "$DEFDT_VERSION_FILE" | sed -e 's/ //g'`
fi
if test -x "$PREBUILT_DATATOOL_EXE"; then
  ptbver=`$PREBUILT_DATATOOL_EXE -version | grep ^$DT | sed -e s/$DT:// | sed -e 's/ //g'`
  if ! test "$ptbver" = "$DEFDT_VERSION"; then
    echo "WARNING: requested $DT version $ptbver does not match default one: $DEFDT_VERSION"
    DEFDT_VERSION=$ptbver
  fi
fi

# -------------------------------------------------------------------------
# Identify DATATOOL_EXE

if test "$PREBUILT_DATATOOL_EXE" = "bootstrap"; then
  DEF_DT="$DATATOOL_PATH/$DT"
else
  if test -n "$PREBUILT_DATATOOL_EXE"; then
    if test -x "$PREBUILT_DATATOOL_EXE"; then
      DEF_DT="$PREBUILT_DATATOOL_EXE"
    else
      echo error: $PREBUILT_DATATOOL_EXE not found
      exit 1
    fi
  else
    DEF_DT="$DEFDT_LOCATION/XCode/$DEFDT_VERSION/$DT"
  fi
fi

if test -x "$DEF_DT"; then
  DATATOOL_EXE="$DEF_DT"
else
  echo "$DT not found at $DEF_DT"
  DATATOOL_EXE="$DATATOOL_PATH/$DT"
fi

# -------------------------------------------------------------------------
# Build DATATOOL_EXE if needed

if test ! -x "$DATATOOL_EXE"; then
  echo "=============================================================================="
  echo Building $DT locally, please wait
  cmd="`dirname $0`/xcodebuild.sh -project $PTB_SLN -target $DT -configuration ReleaseDLL"
  echo "$cmd"
  echo "=============================================================================="
  $cmd
else
  echo "=============================================================================="
  echo "Using PREBUILT $DT at $DATATOOL_EXE"
  echo "=============================================================================="
fi

if test ! -x "$DATATOOL_EXE"; then
  echo "error: $DT not found at $DATATOOL_EXE"
  exit 1
fi
$DATATOOL_EXE -version
if test $? -ne 0; then
  echo "error: cannot find working $DT"
  exit 1
fi

# -------------------------------------------------------------------------
# Run DATATOOL_EXE

$DATATOOL_EXE "$@"
if test $? -ne 0; then
  exit 1
fi
exit 0
