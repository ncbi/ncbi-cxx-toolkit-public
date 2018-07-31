#!/bin/sh
#############################################################################
# $Id$
#   Configure NCBI C++ toolkit for CMake build system.
#############################################################################
initial_dir=`pwd`
script_name=`basename $0`
script_dir=`dirname $0`
script_dir=`(cd "${script_dir}" ; pwd)`
tree_root=`pwd`

############################################################################# 
if [ -z "${CMAKE_CMD}" ]; then
    CMAKE_CMD="cmake"
fi
CMAKE_ARGS=""
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
  CC_NAME=`$CC --version | awk 'NR==1{print $2}' | sed 's/[()]//g'`
  CC_VERSION=`$CC --version | awk 'NR==1{print $3}' | sed 's/[.]//g'`
fi

############################################################################# 
# defaults
BUILD_TYPE="Debug"
BUILD_SHARED_LIBS="OFF"
USE_CCACHE="OFF"
USE_DISTCC="OFF"

############################################################################# 
Usage()
{
    cat <<EOF 1>&2
USAGE:
  $script_name [OPTION]...
SYNOPSIS:
  Configure NCBI C++ toolkit for CMake build system.
OPTIONS:
  --help                     -- print Usage
  --without-debug            -- build release versions of libs and apps
  --with-debug               -- build debug versions of libs and apps
  --without-dll              -- build all libraries as static ones
  --with-dll                 -- build all libraries as shared ones,
                                unless explicitely requested otherwise
  --with-ccache              -- use ccache if available
  --with-distcc              -- use distcc if available
  --with-projects='FILE'     -- build projects listed in ${tree_root}/FILE
                                FILE can also be a list of subdirectories of ${tree_root}/src
                    examples:   --with-projects='corelib$;serial'
                                --with-projects=scripts/projects/ncbi_cpp.lst
  --with-generator='X'       -- use generator X
EOF
  echo
  echo The following generators are available on this platform:
  $CMAKE_CMD --help | grep Generates
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
    --with-generator=*)
      generator=${1#*=}
      ;; 
    --with-projects=*)
      project_list=${1#*=}
      if [ -e "${tree_root}/$project_list" ]; then
        project_list="${tree_root}/$project_list"
      fi
      ;; 
    *) 
      Error "unknown option: $1" 
      ;; 
  esac 
  shift 
done 

############################################################################# 

if [ -n "$CC" ]; then
  CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_C_COMPILER=$(Quote "$CC")"
fi
if [ -n "$CXX" ]; then
  CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_CXX_COMPILER=$(Quote "$CXX")"
fi
if [ -n "$generator" ]; then
  CMAKE_ARGS="$CMAKE_ARGS -G $(Quote "$generator")"
fi
if [ -n "$project_list" ]; then
  CMAKE_ARGS="$CMAKE_ARGS  -DNCBI_PROJECT_LIST=$(Quote "${project_list}")"
fi
CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_BUILD_TYPE=${BUILD_TYPE}"
CMAKE_ARGS="$CMAKE_ARGS -DBUILD_SHARED_LIBS=$BUILD_SHARED_LIBS"
CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_USE_CCACHE=$USE_CCACHE"
CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_USE_DISTCC=$USE_DISTCC"
build_root=CMake-${CC_NAME}${CC_VERSION}-${BUILD_TYPE}
if [ "$BUILD_SHARED_LIBS" == "ON" ]; then
  build_root="$build_root"DLL
fi
#build_root="$build_root"64

mkdir -p ${tree_root}/${build_root}/build 
cd ${tree_root}/${build_root}/build 

#we need to do this because CMake caches configuration parameters,
#and if you run ./cmake-configure for existing build directory
#but with another parameters, CMake will use old values for paramers you omit (not default ones)
if [ -e CMakeCache.txt ]; then
   rm CMakeCache.txt
fi

echo Running "${CMAKE_CMD}" ${CMAKE_ARGS} "../../src"
eval "${CMAKE_CMD}" ${CMAKE_ARGS}  "../../src"
cd ${initial_dir}
