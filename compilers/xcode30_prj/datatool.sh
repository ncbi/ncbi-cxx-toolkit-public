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

for v in "$DATATOOL_PATH" "$TREE_ROOT" "$BUILD_TREE_ROOT"; do
  if test "$v" = ""; then
    echo ERROR: required environment variable is missing
    echo DO NOT ATTEMPT to run this script manually
    exit 1
  fi
done
PTB_SLN="${BUILD_TREE_ROOT}/static/UtilityProjects/PTB.xcodeproj"
DT="datatool"

DATATOOL_EXE=$DATATOOL_PATH/$DT
# -------------------------------------------------------------------------
# Build DATATOOL_EXE if needed

if test ! -x "$DATATOOL_EXE"; then
  echo "=============================================================================="
  echo Building $DT locally, please wait
  echo "xcodebuild -project $PTB_SLN -target $DT -configuration ReleaseDLL"
  echo "=============================================================================="
  xcodebuild -project $PTB_SLN -target $DT -configuration ReleaseDLL
else
  echo "=============================================================================="
  echo "Using PREBUILT $DT at $DATATOOL_EXE"
  echo "=============================================================================="
fi

if test ! -x "$DATATOOL_EXE"; then
  echo "ERROR: $DT not found at $DATATOOL_EXE"
  exit 1
fi
$DATATOOL_EXE -version
if test $? -ne 0; then
  echo "ERROR: cannot find working $DT"
  exit 1
fi

# -------------------------------------------------------------------------
# Run DATATOOL_EXE

$DATATOOL_EXE "$@"
