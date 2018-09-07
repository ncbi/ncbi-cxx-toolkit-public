#!/bin/sh
#############################################################################
# $Id$
#   Configure NCBI C++ toolkit using CMake build system.
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
############################################################################# 
if [ -z "${CMAKE_CMD}" ]; then
  CMAKE_CMD=`which cmake 2>/dev/null`
  if test $? -ne 0; then
    echo ERROR: CMake is not found
    exit 1
  fi
fi
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

############################################################################# 
# defaults
BUILD_TYPE="Debug"
BUILD_SHARED_LIBS="OFF"
USE_CCACHE="OFF"
USE_DISTCC="OFF"
NCBI_EXPERIMENTAL=ON

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
  --with-projects='FILE'     -- build projects listed in ${tree_root}/FILE
                                FILE can also be a list of subdirectories of ${tree_root}/src
                    examples:   --with-projects='corelib$;serial'
                                --with-projects=scripts/projects/ncbi_cpp.lst
  --with-tags='tags'         -- build projects which have allowed tags only
                    examples:   --with-tags='*;-test'
  --with-targets='names'     -- build projects which have allowed names only
                    examples:   --with-targets='datatool;xcgi$'
  --with-ccache              -- use ccache if available
  --with-distcc              -- use distcc if available
  --without-experimental     -- disable experimental configuration
  --with-generator='X'       -- use generator X
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

while [ $# != 0 ]; do
  case "$1" in 
    --help)
      Usage
      exit 0
    ;; 
    --srcdir=*)
      tree_root=`(cd "${1#*=}" ; pwd)`
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
    --without-experimental)
      NCBI_EXPERIMENTAL=OFF
      ;;
    --with-generator=*)
      generator=${1#*=}
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
    *) 
      Error "unknown option: $1" 
      ;; 
  esac 
  shift 
done 

############################################################################# 
if test "$generator" = "Xcode"; then
  XC=`which xcodebuild 2>/dev/null`
  if test $? -ne 0; then
    echo ERROR: xcodebuild is not found
    exit 1
  fi
  CC_NAME=Xcode
  CC_VERSION=`xcodebuild -version | awk 'NR==1{print $2}'`
  CC=
  CXX=
fi
############################################################################# 

CMAKE_ARGS=-DNCBI_EXPERIMENTAL=${NCBI_EXPERIMENTAL}

if [ -n "$CC" ]; then
  CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_C_COMPILER=$(Quote "$CC")"
fi
if [ -n "$CXX" ]; then
  CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_CXX_COMPILER=$(Quote "$CXX")"
fi
if [ -n "$generator" ]; then
  CMAKE_ARGS="$CMAKE_ARGS -G $(Quote "$generator")"
fi
CMAKE_ARGS="$CMAKE_ARGS  -DNCBI_PTBCFG_PROJECT_LIST=$(Quote "${project_list}")"
CMAKE_ARGS="$CMAKE_ARGS  -DNCBI_PTBCFG_PROJECT_TAGS=$(Quote "${project_tags}")"
CMAKE_ARGS="$CMAKE_ARGS  -DNCBI_PTBCFG_PROJECT_TARGETS=$(Quote "${project_targets}")"
CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_BUILD_TYPE=${BUILD_TYPE}"
CMAKE_ARGS="$CMAKE_ARGS -DBUILD_SHARED_LIBS=$BUILD_SHARED_LIBS"
if test "$generator" = "Xcode"; then
  build_root=compilers/CMake-${CC_NAME}${CC_VERSION}
  if [ "$BUILD_SHARED_LIBS" == "ON" ]; then
    build_root="$build_root"/dll
    project_name=ncbi_cpp_dll
  else
    build_root="$build_root"/static
    project_name=ncbi_cpp
  fi
  CMAKE_ARGS="$CMAKE_ARGS -DNCBI_CMAKEPROJECT_NAME=$project_name"
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

#we need to do this because CMake caches configuration parameters,
#and if you run ./cmake-configure for existing build directory
#but with another parameters, CMake will use old values for paramers you omit (not default ones)
#if [ -e CMakeCache.txt ]; then
#   rm CMakeCache.txt
#fi

echo Running "${CMAKE_CMD}" ${CMAKE_ARGS} "${tree_root}/src"
eval "${CMAKE_CMD}" ${CMAKE_ARGS}  "${tree_root}/src"
cd ${initial_dir}
