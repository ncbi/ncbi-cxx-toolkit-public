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
extension="cmake_configure_ext.sh"

############################################################################# 
if [ -z "${CMAKE_CMD}" ]; then
  CMAKE_CMD=/Applications/CMake.app/Contents/bin/cmake
fi
if test ! -x $CMAKE_CMD; then
  CMAKE_CMD=/sw/bin/cmake
  if test ! -x $CMAKE_CMD; then
    CMAKE_CMD=`which cmake 2>/dev/null`
    if test $? -ne 0; then
      echo ERROR: CMake is not found 1>&2
      exit 1
    fi
  fi
fi
$CMAKE_CMD --version

############################################################################# 
# defaults
BUILD_SHARED_LIBS="OFF"
SKIP_ANALYSIS="OFF"

############################################################################# 
Check_function_exists() {
  local t=`type -t $1`
  test "$t" = "function"
}

Usage()
{
    cat <<EOF
USAGE:
  $script_name [OPTIONS]...
SYNOPSIS:
  Configure NCBI C++ toolkit for XCode using CMake build system.
  https://ncbi.github.io/cxx-toolkit/pages/ch_cmconfig#ch_cmconfig._Configure
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
  --with-details="names"     -- print detailed information about projects
                    examples:   --with-details="datatool;test_hash"
  --with-install="DIR"       -- generate rules for installation into DIR directory
                    examples:   --with-install="/usr/CPP_toolkit"
  --with-components="LIST"   -- explicitly enable or disable components
                    examples:   --with-components="-Z"
  --with-features="LIST"     -- specify compilation features
                    examples:   --with-features="StrictGI"
  --with-build-root=name     -- specify a non-default build directory name
  --without-analysis         -- skip source tree analysis
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
  if [ -n "$1" ]; then
    echo ----------------------------------------------------------------------
    echo ERROR: $1 1>&2
  fi
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
    --without-dll) 
      BUILD_SHARED_LIBS=OFF
      ;; 
    --with-dll) 
      BUILD_SHARED_LIBS=ON 
      ;; 
    --with-projects=*)
      PROJECT_LIST=${1#*=}
      if [ -f "${tree_root}/$PROJECT_LIST" ]; then
        PROJECT_LIST="${tree_root}/$PROJECT_LIST"
      fi
      ;; 
    --with-tags=*)
      PROJECT_TAGS=${1#*=}
      if [ -f "${tree_root}/$PROJECT_TAGS" ]; then
        PROJECT_TAGS="${tree_root}/$PROJECT_TAGS"
      fi
      ;; 
    --with-targets=*)
      PROJECT_TARGETS=${1#*=}
      if [ -f "${tree_root}/$PROJECT_TARGETS" ]; then
        PROJECT_TARGETS="${tree_root}/$PROJECT_TARGETS"
      fi
      ;; 
    --with-details=*)
      PROJECT_DETAILS=${1#*=}
      ;; 
    --with-install=*)
      INSTALL_PATH=${1#*=}
      ;; 
    --with-components=*)
      PROJECT_COMPONENTS=${1#*=}
      ;; 
    --with-features=*)
      PROJECT_FEATURES=${1#*=}
      ;; 
    --with-build-root=*)
      BUILD_ROOT=${1#*=}
      ;; 
    --with-prebuilt=*)
      prebuilt_path=${1#*=}
      prebuilt_dir=`dirname $prebuilt_path`
      prebuilt_name=`basename $prebuilt_path`
      ;; 
    --without-analysis)
      SKIP_ANALYSIS="ON"
      ;;
    [A-Z]*)
      ;; 
    -D*)
      CMAKE_ARGS="$CMAKE_ARGS $1"
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
    Error "Buildinfo not found in $prebuilt_dir/$prebuilt_name"
  fi
fi

############################################################################# 
XC=`which xcodebuild 2>/dev/null`
if test $? -ne 0; then
  echo ERROR: xcodebuild is not found 1>&2
  exit 1
fi
CC_NAME=Xcode
CC_VERSION=`xcodebuild -version | awk 'NR==1{print $2}'`
############################################################################# 

CMAKE_ARGS="$CMAKE_ARGS -G Xcode"

CMAKE_ARGS="$CMAKE_ARGS  -DNCBI_PTBCFG_PROJECT_COMPONENTS=$(Quote "${PROJECT_COMPONENTS}")"
CMAKE_ARGS="$CMAKE_ARGS  -DNCBI_PTBCFG_PROJECT_FEATURES=$(Quote "${PROJECT_FEATURES}")"
CMAKE_ARGS="$CMAKE_ARGS  -DNCBI_PTBCFG_PROJECT_LIST=$(Quote "${PROJECT_LIST}")"
CMAKE_ARGS="$CMAKE_ARGS  -DNCBI_PTBCFG_PROJECT_TAGS=$(Quote "${PROJECT_TAGS}")"
CMAKE_ARGS="$CMAKE_ARGS  -DNCBI_PTBCFG_PROJECT_TARGETS=$(Quote "${PROJECT_TARGETS}")"
CMAKE_ARGS="$CMAKE_ARGS  -DNCBI_VERBOSE_PROJECTS=$(Quote "${PROJECT_DETAILS}")"
CMAKE_ARGS="$CMAKE_ARGS  -DNCBI_PTBCFG_SKIP_ANALYSIS=$(Quote "${SKIP_ANALYSIS}")"
if [ -n "$INSTALL_PATH" ]; then
  CMAKE_ARGS="$CMAKE_ARGS  -DNCBI_PTBCFG_INSTALL_PATH=$(Quote "${INSTALL_PATH}")"
fi
CMAKE_ARGS="$CMAKE_ARGS -DBUILD_SHARED_LIBS=$BUILD_SHARED_LIBS"

if [ -z "$BUILD_ROOT" ]; then
  BUILD_ROOT=CMake-${CC_NAME}${CC_VERSION}
  if [ "$BUILD_SHARED_LIBS" == "ON" ]; then
    BUILD_ROOT="$BUILD_ROOT"-DLL
  fi
fi

cd ${tree_root}
Check_function_exists configure_ext_PreCMake && configure_ext_PreCMake
mkdir -p ${BUILD_ROOT}/build 
if [ ! -d ${BUILD_ROOT}/build ]; then
  Error "Failed to create directory: ${BUILD_ROOT}/build"
fi
cd ${BUILD_ROOT}/build 

#echo Running "${CMAKE_CMD}" ${CMAKE_ARGS} "${tree_root}/src"
if [ -f "${tree_root}/CMakeLists.txt" ]; then
  eval "${CMAKE_CMD}" ${CMAKE_ARGS}  "${tree_root}"
else
  eval "${CMAKE_CMD}" ${CMAKE_ARGS}  "${tree_root}/src"
fi
if test $? -ne 0; then
    exit 1
fi
cd ${initial_dir}
