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
Usage()
{
    cat <<EOF 1>&2
USAGE:
  $script_name [OPTION]...
SYNOPSIS:
  Configure NCBI C++ toolkit using CMake build system.
OPTIONS:
  --help                     -- print Usage
  --without-debug            -- build release versions of libs and apps
  --with-debug               -- build debug versions of libs and apps (default)
  --without-dll              -- build all libraries as static ones (default)
  --with-dll                 -- build all libraries as shared ones,
                                unless explicitely requested otherwise
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
  --without-ccache           -- do not use ccache
  --without-distcc           -- do not use distcc
  --with-generator="X"       -- use generator X
EOF

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
while [ $# != 0 ]; do
  case "$1" in 
    --help|-help|help)
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
    --with-generator=*)
      CMAKE_GENERATOR=${1#*=}
      ;; 
    --with-projects=*)
      project_list=${1#*=}
      if [ -e "${tree_root}/$project_list" ]; then
        project_list="${tree_root}/$project_list"
      fi
      ;; 
    --with-tags=*)
      project_tags=${1#*=}
      if [ -e "${tree_root}/$project_tags" ]; then
        project_tags="${tree_root}/$project_tags"
      fi
      ;; 
    --with-targets=*)
      project_targets=${1#*=}
      if [ -e "${tree_root}/$project_targets" ]; then
        project_targets="${tree_root}/$project_targets"
      fi
      ;; 
    --with-details=*)
      project_details=${1#*=}
      ;; 
    --with-install=*)
      install_path=${1#*=}
      ;; 
    --with-prebuilt=*)
      prebuilt_path=${1#*=}
      prebuilt_dir=`dirname $prebuilt_path`
      prebuilt_name=`basename $prebuilt_path`
      ;; 
    *) 
      Error "unknown option: $1" 
      ;; 
  esac 
  shift 
done 
if [ $do_help = "yes" ]; then
  Usage
  exit 0
fi

############################################################################# 
if [ -n $prebuilt_dir ]; then
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

CMAKE_ARGS=-DNCBI_EXPERIMENTAL=ON

if [ -n "$CC" ]; then
  CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_C_COMPILER=$(Quote "$CC")"
fi
if [ -n "$CXX" ]; then
  CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_CXX_COMPILER=$(Quote "$CXX")"
fi
if [ -n "$CMAKE_GENERATOR" ]; then
  CMAKE_ARGS="$CMAKE_ARGS -G $(Quote "$CMAKE_GENERATOR")"
fi
CMAKE_ARGS="$CMAKE_ARGS  -DNCBI_PTBCFG_PROJECT_LIST=$(Quote "${project_list}")"
CMAKE_ARGS="$CMAKE_ARGS  -DNCBI_PTBCFG_PROJECT_TAGS=$(Quote "${project_tags}")"
CMAKE_ARGS="$CMAKE_ARGS  -DNCBI_PTBCFG_PROJECT_TARGETS=$(Quote "${project_targets}")"
CMAKE_ARGS="$CMAKE_ARGS  -DNCBI_VERBOSE_PROJECTS=$(Quote "${project_details}")"
if [ -n "$install_path" ]; then
  CMAKE_ARGS="$CMAKE_ARGS  -DNCBI_PTBCFG_INSTALL_PATH=$(Quote "${install_path}")"
fi
CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_BUILD_TYPE=${BUILD_TYPE}"
CMAKE_ARGS="$CMAKE_ARGS -DBUILD_SHARED_LIBS=$BUILD_SHARED_LIBS"
if test "$CMAKE_GENERATOR" = "Xcode"; then
  build_root=CMake-${CC_NAME}${CC_VERSION}
  if [ "$BUILD_SHARED_LIBS" == "ON" ]; then
    build_root="$build_root"-DLL
  fi
else
  CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_USE_CCACHE=$USE_CCACHE"
  CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_USE_DISTCC=$USE_DISTCC"
  build_root=CMake-${CC_NAME}${CC_VERSION}-${BUILD_TYPE}
  if [ "$BUILD_SHARED_LIBS" == "ON" ]; then
    build_root="$build_root"DLL
  fi
#build_root="$build_root"64
fi

mkdir -p ${tree_root}/${build_root}/build 
cd ${tree_root}/${build_root}/build 

#echo Running "${CMAKE_CMD}" ${CMAKE_ARGS} "${tree_root}/src"
eval "${CMAKE_CMD}" ${CMAKE_ARGS}  "${tree_root}/src"
cd ${initial_dir}
