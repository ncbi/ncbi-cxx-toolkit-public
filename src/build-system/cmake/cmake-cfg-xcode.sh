#!/bin/sh
#############################################################################
# $Id$
#   Configure NCBI C++ toolkit for XCode using CMake build system.
#   Author: Andrei Gourianov, gouriano@ncbi
#############################################################################
initial_dir=`pwd`
script_name=`basename $0`
script_dir=`dirname $0`
script_dir=`(cd "${script_dir}" ; pwd)`
tree_root=`pwd`

############################################################################# 
if [ -z "${CMAKE_CMD}" ]; then
  CMAKE_CMD=/Applications/CMake.app/Contents/bin/cmake
fi
if test ! -x $CMAKE_CMD; then
  CMAKE_CMD=`which cmake 2>/dev/null`
  if test $? -ne 0; then
    echo ERROR: CMake is not found
    exit 1
  fi
fi

############################################################################# 
# defaults
BUILD_SHARED_LIBS="OFF"

############################################################################# 
Usage()
{
    cat <<EOF 1>&2
USAGE:
  $script_name [OPTIONS]...
SYNOPSIS:
  Configure NCBI C++ toolkit for XCode using CMake build system.
OPTIONS:
  --help                     -- print Usage
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
  --with-components="LIST"   -- explicitly enable or disable components
                    examples:   --with-components="StrictGI;-Z"
  --with-details="names"     -- print detailed information about projects
                    examples:   --with-details="datatool;test_hash"
  --with-install="DIR"       -- generate rules for installation into DIR directory
                    examples:   --with-install="/usr/CPP_toolkit"
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
    --without-dll) 
      BUILD_SHARED_LIBS=OFF
      ;; 
    --with-dll) 
      BUILD_SHARED_LIBS=ON 
      ;; 
    --with-components=*)
      PROJECT_COMPONENTS=${1#*=}
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
if [ -n "$prebuilt_dir" ]; then
  if [ -f $prebuilt_dir/$prebuilt_name/cmake/buildinfo ]; then
    source $prebuilt_dir/$prebuilt_name/cmake/buildinfo
  else
    echo ERROR:  Buildinfo not found in $prebuilt_dir/$prebuilt_name
    exit 1
  fi
fi

############################################################################# 
XC=`which xcodebuild 2>/dev/null`
if test $? -ne 0; then
  echo ERROR: xcodebuild is not found
  exit 1
fi
CC_NAME=Xcode
CC_VERSION=`xcodebuild -version | awk 'NR==1{print $2}'`
############################################################################# 

CMAKE_ARGS="-DNCBI_EXPERIMENTAL=ON -G Xcode"

CMAKE_ARGS="$CMAKE_ARGS  -DNCBI_PTBCFG_PROJECT_COMPONENTS=$(Quote "${PROJECT_COMPONENTS}")"
CMAKE_ARGS="$CMAKE_ARGS  -DNCBI_PTBCFG_PROJECT_LIST=$(Quote "${PROJECT_LIST}")"
CMAKE_ARGS="$CMAKE_ARGS  -DNCBI_PTBCFG_PROJECT_TAGS=$(Quote "${PROJECT_TAGS}")"
CMAKE_ARGS="$CMAKE_ARGS  -DNCBI_PTBCFG_PROJECT_TARGETS=$(Quote "${PROJECT_TARGETS}")"
CMAKE_ARGS="$CMAKE_ARGS  -DNCBI_VERBOSE_PROJECTS=$(Quote "${PROJECT_DETAILS}")"
if [ -n "$INSTALL_PATH" ]; then
  CMAKE_ARGS="$CMAKE_ARGS  -DNCBI_PTBCFG_INSTALL_PATH=$(Quote "${INSTALL_PATH}")"
fi
CMAKE_ARGS="$CMAKE_ARGS -DBUILD_SHARED_LIBS=$BUILD_SHARED_LIBS"

BUILD_ROOT=CMake-${CC_NAME}${CC_VERSION}
if [ "$BUILD_SHARED_LIBS" == "ON" ]; then
  BUILD_ROOT="$BUILD_ROOT"-DLL
fi

if test ! -e "${tree_root}/${BUILD_ROOT}/build"; then
  mkdir -p "${tree_root}/${BUILD_ROOT}/build"
fi
cd ${tree_root}/${BUILD_ROOT}/build 


#echo Running "${CMAKE_CMD}" ${CMAKE_ARGS} "${tree_root}/src"
eval "${CMAKE_CMD}" ${CMAKE_ARGS}  "${tree_root}/src"
cd ${initial_dir}
