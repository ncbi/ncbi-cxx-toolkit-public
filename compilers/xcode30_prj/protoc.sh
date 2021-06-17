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

PROTOC_APP=protoc
GRPC_APP=grpc_cpp_plugin

# remove the following after the transition period!
if test -z "$GENERATOR_PATH"; then
  GENERATOR_PATH=/netopt/ncbi_tools/grpc-1.14.0-ncbi1/Release/bin
fi

case ":$GENERATOR_PATH:$TREE_ROOT:$BUILD_TREE_ROOT:$PTB_PLATFORM:" in
    *::* )
        echo '$0: ERROR: required environment variable is missing.' >&2
        echo 'DO NOT ATTEMPT to run this script manually.' >&2
        exit 1
        ;;
esac

PROTOC=$GENERATOR_PATH/$PROTOC_APP
GRPC_PLUGIN=$GENERATOR_PATH/$GRPC_APP

input_spec_path=
input_spec_name=
input_def_path=
subtree=
srcroot=

while [ $# != 0 ]; do
    case "$1" in
        -m )
            input_spec_name="$(basename "$2")"
	    input_spec_base="$(basename "$2" .proto)"
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
    cd "$TREE_ROOT/src"
    if [ -x "$PROTOC" ]; then
        "$PROTOC" --cpp_out=. -I. "$subtree/$input_spec_name"
    else
        touch $subtree/$input_spec_name.pb.cc \
              ../include/$subtree/$input_spec_name.pb.h
    fi
    if [ -x "$PROTOC" -a -x "$GRPC_PLUGIN" ]; then
        "$PROTOC" --grpc_out=generate_mock_code=true:. \
                  --plugin=protoc-gen-grpc="$GRPC_PLUGIN" -I. \
                  "$subtree/$input_spec_name"
    else
        touch $subtree/$input_spec_name.grpc.pb.cc \
              ../include/$subtree/$input_spec_name.grpc.pb.h \
              ../include/$subtree/$input_spec_name_mock.grpc.pb.h
    fi
    )
mkdir -p "$TREE_ROOT/include/$subtree"
mv -f "$TREE_ROOT/src/$subtree/"*.pb.h "$TREE_ROOT/include/$subtree/"
