#!/bin/sh
#############################################################################
# $Id$
#   Configure NCBI C++ toolkit using CMake build system.
#   Author: Andrei Gourianov, gouriano@ncbi
#############################################################################
initial_dir=`pwd`
script_name=`basename $0`
script_dir=`dirname $0`
script_dir=`(cd "${script_dir}" ; pwd)`
tree_root=`pwd`
extension="cmake_configure_ext.sh"
NCBI_EXPERIMENTAL="ON"

host_os=`uname`
if test $host_os = "Darwin"; then
  CMAKE_CMD=/Applications/CMake.app/Contents/bin/cmake
fi
if [ -z "${CMAKE_CMD}" ]; then
  CMAKE_CMD=`which cmake 2>/dev/null`
  if test $? -ne 0; then
    echo ERROR: CMake is not found
    exit 1
  fi
fi

############################################################################# 
# defaults
BUILD_TYPE="Debug"
BUILD_SHARED_LIBS="OFF"
USE_CCACHE="ON"
USE_DISTCC="ON"

############################################################################# 
Check_function_exists() {
  local t=`type -t $1`
  test "$t" = "function"
}

Usage()
{
    cat <<EOF 1>&2
USAGE:
  $script_name [OPTIONS]...
SYNOPSIS:
  Configure NCBI C++ toolkit using CMake build system.
OPTIONS:
  --help                     -- print Usage
  --without-debug            -- build release versions of libs and apps
  --with-debug               -- build debug versions of libs and apps (default)
  --with-max-debug           -- enable extra runtime checks (esp. of STL usage)
  --with-symbols             -- retain debugging symbols in non-debug mode
  --without-dll              -- build all libraries as static ones (default)
  --with-dll                 -- build all libraries as shared ones,
                                unless explicitly requested otherwise
  --with-projects="FILE"     -- build projects listed in ${tree_root}/FILE
                                FILE can also be a list of subdirectories of ${tree_root}/src
                    examples:   --with-projects="corelib$;serial"
                                --with-projects=scripts/projects/ncbi_cpp.lst
  --with-tags="tags"         -- build projects which have allowed tags only
                    examples:   --with-tags="*;-test"
  --with-targets="names"     -- build projects which have allowed names only
                    examples:   --with-targets="datatool;xcgi$"
  --with-details="names"     -- print detailed information about projects
                    examples:   --with-details="datatool;test_hash"
  --with-install="DIR"       -- generate rules for installation into DIR directory
                    examples:   --with-install="/usr/CPP_toolkit"
  --with-components="LIST"   -- explicitly enable or disable components
                    examples:   --with-components="-Z"
  --with-features="LIST"     -- specify compilation features
                    examples:   --with-features="StrictGI"
  --with-build-root=name     -- specify a non-default build directory name
  --without-ccache           -- do not use ccache
  --without-distcc           -- do not use distcc
  --with-generator="X"       -- use generator X
EOF

  Check_function_exists configure_ext_Usage && configure_ext_Usage

  generatorfound=""
  "${CMAKE_CMD}" --help | while IFS= read -r line; do
    if [ -z $generatorfound ]; then
      if [ "$line" = "Generators" ]; then
        generatorfound=yes
      fi
    else
      echo "$line"
    fi
  done
}

# has one optional argument: error message
Error()
{
  Usage
  test -z "$1"  ||  echo ERROR: $1 1>&2
  cd "$initial_dir"
  exit 1
}

Quote() {
    echo "$1" | sed -e "s|'|'\\\\''|g; 1s/^/'/; \$s/\$/'/"
}

############################################################################# 
# parse arguments

do_help="no"
unknown=""
while [ $# != 0 ]; do
  case "$1" in 
    --help|-help|help|-h)
      do_help="yes"
    ;; 
    --rootdir=*)
      tree_root=`(cd "${1#*=}" ; pwd)`
      ;; 
    --caller=*)
      script_name=${1#*=}
      ;; 
    --with-static|--without-dll) 
      BUILD_SHARED_LIBS=OFF
      ;; 
    --with-dll) 
      BUILD_SHARED_LIBS=ON 
      ;; 
    --with-debug) 
      BUILD_TYPE=Debug 
      ;; 
    --without-debug) 
      BUILD_TYPE=Release 
      ;; 
    --with-max-debug)
      BUILD_TYPE=Debug 
      PROJECT_FEATURES="$PROJECT_FEATURES;MaxDebug"
      ;; 
    --with-symbols)
      PROJECT_FEATURES="$PROJECT_FEATURES;Symbols"
      ;; 
    --with-ccache)
      USE_CCACHE="ON"
      ;; 
    --without-ccache)
      USE_CCACHE="OFF"
      ;; 
    --with-distcc)
      USE_DISTCC="ON"
      ;; 
    --without-distcc)
      USE_DISTCC="OFF"
      ;;
    --with-projects=*)
      PROJECT_LIST=${1#*=}
      if [ -e "${tree_root}/$PROJECT_LIST" ]; then
        PROJECT_LIST="${tree_root}/$PROJECT_LIST"
      fi
      ;; 
    --with-tags=*)
      PROJECT_TAGS=${1#*=}
      if [ -e "${tree_root}/$PROJECT_TAGS" ]; then
        PROJECT_TAGS="${tree_root}/$PROJECT_TAGS"
      fi
      ;; 
    --with-targets=*)
      PROJECT_TARGETS=${1#*=}
      if [ -e "${tree_root}/$PROJECT_TARGETS" ]; then
        PROJECT_TARGETS="${tree_root}/$PROJECT_TARGETS"
      fi
      ;; 
    --with-details=*)
      PROJECT_DETAILS=${1#*=}
      ;; 
    --with-install=*)
      INSTALL_PATH=${1#*=}
      ;; 
    --with-generator=*)
      CMAKE_GENERATOR=${1#*=}
      ;; 
    --with-components=*)
      PROJECT_COMPONENTS=${1#*=}
      ;; 
    --with-features=*)
      PROJECT_FEATURES="$PROJECT_FEATURES;${1#*=}"
      ;; 
    --with-build-root=*)
      BUILD_ROOT=${1#*=}
      ;; 
    --with-prebuilt=*)
      prebuilt_path=${1#*=}
      prebuilt_dir=`dirname $prebuilt_path`
      prebuilt_name=`basename $prebuilt_path`
      ;; 
    *) 
      unknown="$unknown $1"
      ;; 
  esac 
  shift 
done 

if [ -f $tree_root/$extension ]; then
  source $tree_root/$extension
fi

if [ $do_help = "yes" ]; then
  Usage
  exit 0
fi

if [ -n "$unknown" ]; then
  Check_function_exists configure_ext_ParseArgs
  if [ $? -eq 0 ]; then
    configure_ext_ParseArgs unknown $unknown
  fi
fi
if [ -n "$unknown" ]; then
  Error "Unknown options: $unknown" 
fi

############################################################################# 
if [ -n "$prebuilt_dir" ]; then
  if [ -f $prebuilt_dir/$prebuilt_name/cmake/buildinfo ]; then
    source $prebuilt_dir/$prebuilt_name/cmake/buildinfo
  else
    echo ERROR:  Buildinfo not found in $prebuilt_dir/$prebuilt_name
    exit 1
  fi
fi

############################################################################# 
if test "$CMAKE_GENERATOR" = "Xcode"; then
  XC=`which xcodebuild 2>/dev/null`
  if test $? -ne 0; then
    echo ERROR: xcodebuild is not found
    exit 1
  fi
  CC_NAME=Xcode
  CC_VERSION=`xcodebuild -version | awk 'NR==1{print $2}'`
  CC=
  CXX=
else
  CC=$CMAKE_C_COMPILER
  CXX=$CMAKE_CXX_COMPILER
  if [ -z "$CC" ]; then
    CC=`which gcc 2>/dev/null`
    if test $? -ne 0; then
      CC=""
    fi
  fi
  if [ -z "$CXX" ]; then
    CXX=`which g++ 2>/dev/null`
    if test $? -ne 0; then
      CXX=""
    fi
  fi
  if [ -n "$CC" ]; then
    if test $host_os = "Darwin"; then
      CC_NAME=`$CC --version 2>/dev/null | awk 'NR==1{print $2}'`
      CC_VERSION=`$CC --version 2>/dev/null | awk 'NR==1{print $4}' | sed 's/[.]//g'`
    else
      CC_NAME=`$CC --version | awk 'NR==1{print $2}' | sed 's/[()]//g'`
      CC_VERSION=`$CC --version | awk 'NR==1{print $3}' | sed 's/[.]//g'`
    fi
  fi
fi
############################################################################# 

CMAKE_ARGS=-DNCBI_EXPERIMENTAL=$NCBI_EXPERIMENTAL

if [ -n "$CC" ]; then
  CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_C_COMPILER=$(Quote "$CC")"
fi
if [ -n "$CXX" ]; then
  CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_CXX_COMPILER=$(Quote "$CXX")"
fi
if [ -n "$CMAKE_GENERATOR" ]; then
  CMAKE_ARGS="$CMAKE_ARGS -G $(Quote "$CMAKE_GENERATOR")"
fi
CMAKE_ARGS="$CMAKE_ARGS  -DNCBI_PTBCFG_PROJECT_COMPONENTS=$(Quote "${PROJECT_COMPONENTS}")"
CMAKE_ARGS="$CMAKE_ARGS  -DNCBI_PTBCFG_PROJECT_FEATURES=$(Quote "${PROJECT_FEATURES}")"
CMAKE_ARGS="$CMAKE_ARGS  -DNCBI_PTBCFG_PROJECT_LIST=$(Quote "${PROJECT_LIST}")"
CMAKE_ARGS="$CMAKE_ARGS  -DNCBI_PTBCFG_PROJECT_TAGS=$(Quote "${PROJECT_TAGS}")"
CMAKE_ARGS="$CMAKE_ARGS  -DNCBI_PTBCFG_PROJECT_TARGETS=$(Quote "${PROJECT_TARGETS}")"
CMAKE_ARGS="$CMAKE_ARGS  -DNCBI_VERBOSE_PROJECTS=$(Quote "${PROJECT_DETAILS}")"
if [ -n "$INSTALL_PATH" ]; then
  CMAKE_ARGS="$CMAKE_ARGS  -DNCBI_PTBCFG_INSTALL_PATH=$(Quote "${INSTALL_PATH}")"
fi
CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_BUILD_TYPE=${BUILD_TYPE}"
CMAKE_ARGS="$CMAKE_ARGS -DBUILD_SHARED_LIBS=$BUILD_SHARED_LIBS"
if test "$CMAKE_GENERATOR" != "Xcode"; then
  CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_USE_CCACHE=$USE_CCACHE"
  CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_USE_DISTCC=$USE_DISTCC"
fi

if [ -z "$BUILD_ROOT" ]; then
  if test "$CMAKE_GENERATOR" = "Xcode"; then
    BUILD_ROOT=CMake-${CC_NAME}${CC_VERSION}
    if [ "$BUILD_SHARED_LIBS" == "ON" ]; then
      BUILD_ROOT="$BUILD_ROOT"-DLL
    fi
  else
    BUILD_ROOT=CMake-${CC_NAME}${CC_VERSION}-${BUILD_TYPE}
    if [ "$BUILD_SHARED_LIBS" == "ON" ]; then
      BUILD_ROOT="$BUILD_ROOT"DLL
    fi
#BUILD_ROOT="$BUILD_ROOT"64
  fi
fi

cd ${tree_root}
Check_function_exists configure_ext_PreCMake && configure_ext_PreCMake
mkdir -p ${BUILD_ROOT}/build 
cd ${BUILD_ROOT}/build 

#echo Running "${CMAKE_CMD}" ${CMAKE_ARGS} "${tree_root}/src"
eval "${CMAKE_CMD}" ${CMAKE_ARGS}  "${tree_root}/src"
cd ${initial_dir}
