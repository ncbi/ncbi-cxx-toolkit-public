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
# This script is called before building CONFIGURE target
# It is unlikely that you might want to run this script manually
# ===========================================================================

for v in "$CONFIGURATION" "$BUILD_ROOT" "$PROJECT_FILE_PATH"; do
  if test "$v" = ""; then
    echo error: required environment variable is missing
    exit 1
  fi
done

script_dir=`dirname $0`
project_dir=`dirname $PROJECT_FILE_PATH`
ptb_exe=$script_dir/static/bin/ReleaseDLL/project_tree_builder
ptb_custom=$project_dir/project_tree_builder.ini.custom

if test "$ACTION" = "clean"; then
  test -e "$ptb_exe" && rm -f "$ptb_exe"
  test -e "$ptb_custom" && rm -f "$ptb_custom"
fi
exit 0
