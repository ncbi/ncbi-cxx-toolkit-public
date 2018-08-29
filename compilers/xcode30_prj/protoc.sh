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
# Author: Aaron Ucko (based closely on Andrei Gourianov's protoc.bat)
#
# Run protoc.exe to generate sources from PROTO specification
#
# DO NOT ATTEMPT to run this script manually
#
# ===========================================================================

NCBI=/netopt/ncbi_tools
DEFPROTOC_LOCATION=$NCBI/grpc-1.14.0-ncbi1/Release/bin/protoc
DEFGRPC_LOCATION=$NCBI/grpc-1.14.0-ncbi1/Release/bin/grpc_cpp_plugin

case ":$DATATOOL_PATH:$TREE_ROOT:$BUILD_TREE_ROOT:$PTB_PLATFORM:" in
    *::* )
        echo '$0: ERROR: required environment variable is missing.' >&2
        echo 'DO NOT ATTEMPT to run this script manually.' >&2
        exit 1
        ;;
esac

PROTOC=$DEFPROTOC_LOCATION
GRPC_PLUGIN=$DEFGRPC_LOCATION

for x in "$PROTOC" "$GRPC_PLUGIN"; do
    if [ ! -x "$x" ]; then
        echo "$0: ERROR: $x not found" >&2
        exit 1
    fi
done

input_spec_path=
input_spec_name=
input_def_path=
subtree=
srcroot=

while [ $# != 0 ]; do
    case "$1" in
        -m )
            input_spec_name="$(basename "$2")"
            shift 2
            ;;
        -od )
            input_def_path=$2
            shift 2
            ;;
        -or )
            subtree=$2
            shift 2
            ;;
        -oR )
            srcroot=$2
            shift 2
            ;;
        -M )
            break
            ;;
        -oex | -pch | -oc )
            shift 2
            ;;
        -oA | -odi | -ocvs )
            shift
            ;;
        * )
            echo "$0: Ignoring unsupported argument $1" >&2
            shift
            ;;
    esac
done

(
    cd "$TREE_ROOT/src/$subtree"
    "$PROTOC" --cpp_out=. --proto_path=. "$input_spec_name"
    "$PROTOC" --grpc_out=. --proto_path=. \
              --plugin=protoc-gen-grpc="$GRPC_PLUGIN" \
              "$input_spec_name"
    )
mkdir -p "$TREE_ROOT/include/$subtree"
mv -f "$TREE_ROOT/src/$subtree/"*.pb.h "$TREE_ROOT/include/$subtree/"
