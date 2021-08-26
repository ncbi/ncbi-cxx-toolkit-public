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
# Post-build processing
#
# DO NOT ATTEMPT to run this script manually
# ===========================================================================

for v in "$BUILD_TREE_ROOT"; do
  if test "$v" = ""; then
    echo ERROR: required environment variable is missing
    echo DO NOT ATTEMPT to run this script manually
    exit 1
  fi
done
product=$TARGET_BUILD_DIR/$FULL_PRODUCT_NAME
vdbdlib="${NCBI_VDB_LIBPATH}/vdb/mac/release/x86_64/lib/libncbi-vdb.2.dylib"

if test "$ACTION" = "build"; then
  if test "$MACH_O_TYPE" = "mh_execute" -o "$MACH_O_TYPE" = "mh_dylib"; then
    vdb=`otool -L $product | grep libncbi-vdb | grep dylib`
    if test -n "$vdb"; then
      ref=`echo $vdb | awk 'NR==1{print $1}'`
      install_name_tool -change $ref $vdbdlib $product
    fi
  fi
fi
exit 0

