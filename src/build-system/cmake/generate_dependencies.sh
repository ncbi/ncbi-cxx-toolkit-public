#!/bin/bash

set -e

print_help() {
    cat <<EOF
Usage: $0 --bin <path> [--patch-toolkit] [--with-ccache] [--with-distcc] [--j <num>] [--verbose]

  --bin                   Path to cmake_dependencies_generator binary
  --patch-toolkit         Patch CMake files in C++ toolkit too (by default only in ./internal/ subtree).
  --with-ccache           Use ccache if available
  --with-distcc           Use distcc if available
  --j <num>               Compile in <num> threads
  --verbose               Print names of processed files.
EOF
}

ARGS=""
CMAKE_ARGS=""
MAKE_ARGS=""
CMAKE_DEPENDENCIES_GENERATOR=""

while [ $# != 0 ]; do
    case "$1" in
        "--bin")
            CMAKE_DEPENDENCIES_GENERATOR=`realpath $2`
            shift
            ;;

        "--patch-toolkit")
            ARGS="${ARGS} -patch-toolkit";;

        "--with-ccache")
            CMAKE_ARGS="$CMAKE_ARGS --with-ccache";;

        "--with-distcc")
            CMAKE_ARGS="$CMAKE_ARGS --with-distcc";;

        "--verbose")
            ARGS="${ARGS} --verbose";;

        "-j")
            MAKE_ARGS="${MAKE_ARGS} -j $2"
            shift
            ;;

        "--help")
            print_help;;

        "-h")
            print_help;;

        *)
            echo "$0: error: unrecognized option: \`$1'" >&2
            echo "" >&2
            print_help
            exit -1

    esac;
    shift
done

if [ -z "${CMAKE_DEPENDENCIES_GENERATOR}" ]; then
    print_help
    exit 1;
fi


./cmake-configure --fake-linking --with-build-root=CMake-CompilationOnly ${CMAKE_ARGS}
cd CMake-CompilationOnly/build
make ${MAKE_ARGS}
pwd
${CMAKE_DEPENDENCIES_GENERATOR} -od=.. -cd=../.. ${ARGS}
