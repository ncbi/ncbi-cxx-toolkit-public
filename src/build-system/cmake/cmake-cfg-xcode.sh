#!/bin/sh
#############################################################################
# $Id$
#   Configure NCBI C++ toolkit for XCode using CMake build system.
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
  $script_name [OPTION]...
SYNOPSIS:
  Configure NCBI C++ toolkit using CMake build system.
OPTIONS:
  --help                     -- print Usage
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

generator=Xcode
while [ $# != 0 ]; do
  case "$1" in 
    --help)
      Usage
      exit 0
    ;; 
    --srcdir=*)
      tree_root=`(cd "${1#*=}" ; pwd)`
      ;; 
    --without-dll) 
      BUILD_SHARED_LIBS=OFF
      ;; 
    --with-dll) 
      BUILD_SHARED_LIBS=ON 
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
XC=`which xcodebuild 2>/dev/null`
if test $? -ne 0; then
  echo ERROR: xcodebuild is not found
  exit 1
fi
CC_NAME=Xcode
CC_VERSION=`xcodebuild -version | awk 'NR==1{print $2}'`
############################################################################# 

CMAKE_ARGS="-DNCBI_EXPERIMENTAL=ON -G Xcode"

CMAKE_ARGS="$CMAKE_ARGS -DNCBI_PTBCFG_PROJECT_LIST=$(Quote "${project_list}")"
CMAKE_ARGS="$CMAKE_ARGS  -DNCBI_PTBCFG_PROJECT_TAGS=$(Quote "${project_tags}")"
CMAKE_ARGS="$CMAKE_ARGS  -DNCBI_PTBCFG_PROJECT_TARGETS=$(Quote "${project_targets}")"
CMAKE_ARGS="$CMAKE_ARGS -DBUILD_SHARED_LIBS=$BUILD_SHARED_LIBS"

build_root=CMake-${CC_NAME}${CC_VERSION}
if [ "$BUILD_SHARED_LIBS" == "ON" ]; then
  build_root="$build_root"/dll
  project_name=ncbi_cpp_dll
else
  build_root="$build_root"/static
  project_name=ncbi_cpp
fi
CMAKE_ARGS="$CMAKE_ARGS -DNCBI_CMAKEPROJECT_NAME=$project_name"

if test ! -e "${tree_root}/${build_root}/build"; then
  mkdir -p "${tree_root}/${build_root}/build"
fi
cd ${tree_root}/${build_root}/build 


echo Running "${CMAKE_CMD}" ${CMAKE_ARGS} "${tree_root}/src"
eval "${CMAKE_CMD}" ${CMAKE_ARGS}  "${tree_root}/src"
cd ${initial_dir}
