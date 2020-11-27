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
script_args="$@"
tree_root=`pwd`
extension="cmake_configure_ext.sh"

host_os=`uname`
if test -z "${CMAKE_CMD}" -a $host_os = "Darwin"; then
  CMAKE_CMD=/Applications/CMake.app/Contents/bin/cmake
  if test ! -x $CMAKE_CMD; then
    CMAKE_CMD=/sw/bin/cmake
    if test ! -x $CMAKE_CMD; then
      CMAKE_CMD=""
    fi
  fi
fi
if [ -z "${CMAKE_CMD}" ]; then
  CMAKE_CMD=`which cmake 2>/dev/null`
  if test $? -ne 0; then
    echo ERROR: CMake is not found 1>&2
    exit 1
  fi
fi

############################################################################# 
# defaults
BUILD_TYPE="Debug"
BUILD_SHARED_LIBS="OFF"
USE_CCACHE="ON"
USE_DISTCC="ON"
SKIP_ANALYSIS="OFF"

############################################################################# 
Check_function_exists() {
  t=`type -t $1 2>/dev/null`
  test "$t" = "function"
}

Usage()
{
    cat <<EOF
USAGE:
  $script_name [compiler] [OPTIONS]...
SYNOPSIS:
  Configure NCBI C++ toolkit using CMake build system.
OPTIONS:
  --help                     -- print Usage
  compiler                   -- compiler name and version
                                'name' is one of GCC, ICC, Clang, Xcode
                                'version' depends on 'name'
                    examples:   GCC 7.3.0,  ICC 19.0,  Clang 7.0.0,  Xcode
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
  --without-analysis         -- skip source tree analysis
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
cxx_name=""
cxx_version=""
unknown=""
for arg in ${script_args}
do
  case "$arg" in 
    --help|-help|help|-h)
      do_help="yes"
    ;; 
    --rootdir=*)
      tree_root=`(cd "${arg#*=}" ; pwd)`
      ;; 
    --caller=*)
      script_name=${arg#*=}
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
    --without-analysis)
      SKIP_ANALYSIS="ON"
      ;;
    --with-projects=*)
      PROJECT_LIST=${arg#*=}
      if [ -f "${tree_root}/$PROJECT_LIST" ]; then
        PROJECT_LIST="${tree_root}/$PROJECT_LIST"
      fi
      ;; 
    --with-tags=*)
      PROJECT_TAGS=${arg#*=}
      if [ -f "${tree_root}/$PROJECT_TAGS" ]; then
        PROJECT_TAGS="${tree_root}/$PROJECT_TAGS"
      fi
      ;; 
    --with-targets=*)
      PROJECT_TARGETS=${arg#*=}
      if [ -f "${tree_root}/$PROJECT_TARGETS" ]; then
        PROJECT_TARGETS="${tree_root}/$PROJECT_TARGETS"
      fi
      ;; 
    --with-details=*)
      PROJECT_DETAILS=${arg#*=}
      ;; 
    --with-install=*)
      INSTALL_PATH=${arg#*=}
      ;; 
    --with-generator=*)
      CMAKE_GENERATOR=${arg#*=}
      ;; 
    --with-components=*)
      PROJECT_COMPONENTS=${arg#*=}
      ;; 
    --with-features=*)
      PROJECT_FEATURES="$PROJECT_FEATURES;${arg#*=}"
      ;; 
    --with-build-root=*)
      BUILD_ROOT=${arg#*=}
      ;; 
    --with-prebuilt=*)
      prebuilt_path=${arg#*=}
      prebuilt_dir=`dirname $prebuilt_path`
      prebuilt_name=`basename $prebuilt_path`
      ;; 
    [A-Z]*)
      cxx_name=$arg
      ;; 
    [1-9]*)
      cxx_version=$arg
      ;; 
    -D*)
      CMAKE_ARGS="$CMAKE_ARGS $arg"
      ;; 
    *) 
      unknown="$unknown $arg"
      ;; 
  esac 
done 

if [ -f $tree_root/$extension ]; then
  source $tree_root/$extension
fi

if [ $do_help = "yes" ]; then
  Usage
  exit 0
fi

if [ -z "$CMAKECFGRECURSIONGUARD" -a -n "$cxx_name" ]; then
  if [ "$cxx_name" = "Xcode" ]; then
    exec $script_dir/cmake-cfg-xcode.sh  "$@"
    exit $?
  elif [ -x "$script_dir/cm${cxx_name}.sh" ]; then
    export CMAKECFGRECURSIONGUARD="lock"
    exec $script_dir/cm${cxx_name}.sh  "$@"
    exit $?
  else
    echo ERROR: configuration script for compiler $cxx_name not found 1>&2
    exit 1
  fi
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
  if [ -z "$CC" ]; then
    CC=$CMAKE_C_COMPILER
  fi
  if [ -z "$CXX" ]; then
    CXX=$CMAKE_CXX_COMPILER
  fi
  if [ -z "$CC" ]; then
    CC=`which gcc 2>/dev/null`
    if test $? -ne 0; then
      CC=`which cc 2>/dev/null`
      if test $? -ne 0; then
        CC=""
      fi
    fi
  fi
  if [ -z "$CXX" ]; then
    CXX=`which g++ 2>/dev/null`
    if test $? -ne 0; then
      CXX=`which c++ 2>/dev/null`
      if test $? -ne 0; then
        CXX=""
      fi
    fi
  fi
fi

if [ -n "$CC" ]; then
  ccname=`basename $CC`
  case "$ccname" in
    clang*)
      CC_NAME="Clang"
      if test $host_os = "Darwin"; then
        CC_VERSION=`$CC --version 2>/dev/null | awk 'NR==1{print $4}' | sed 's/[.]//g'`
      else
        CC_VERSION=`$CC --version 2>/dev/null | awk 'NR==1{print $3}' | sed 's/[.]//g'`
      fi
    ;;
    *)
      if test $host_os = "Darwin" -o $host_os = "FreeBSD"; then
        CC_NAME=`$CC --version 2>/dev/null | awk 'NR==1{print $2}'`
        CC_VERSION=`$CC --version 2>/dev/null | awk 'NR==1{print $4}' | sed 's/[.]//g'`
        if [ $CC_NAME = "clang" ]; then
          CC_NAME="Clang"
        fi
      else
        CC_NAME=`$CC --version | awk 'NR==1{print $1}' | tr '[:lower:]' '[:upper:]'`
        ver=`$CC -dumpfullversion 2>/dev/null || $CC -dumpversion 2>/dev/null`
        if [ -n "$ver" ]; then
          CC_VERSION=`echo $ver | awk 'BEGIN{FS="."} { print $1 $2 $3}'`
        else
          CC_VERSION=`$CC --version | awk 'NR==1{print $3}' | sed 's/[.]//g'`
        fi
      fi
    ;;
  esac
else
  CC_NAME="CXX"
  CC_VERSION=""
fi
############################################################################# 

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
CMAKE_ARGS="$CMAKE_ARGS  -DNCBI_PTBCFG_SKIP_ANALYSIS=$(Quote "${SKIP_ANALYSIS}")"
if [ -n "$INSTALL_PATH" ]; then
  CMAKE_ARGS="$CMAKE_ARGS  -DNCBI_PTBCFG_INSTALL_PATH=$(Quote "${INSTALL_PATH}")"
fi
CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_BUILD_TYPE=${BUILD_TYPE}"
CMAKE_ARGS="$CMAKE_ARGS -DBUILD_SHARED_LIBS=$BUILD_SHARED_LIBS"
if test "$CMAKE_GENERATOR" != "Xcode"; then
  CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_USE_CCACHE=$USE_CCACHE"
  CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_USE_DISTCC=$USE_DISTCC"
fi

#CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY"
if [ -n "$NCBI_COMPILER_EXE_LINKER_FLAGS" ]; then
  CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_EXE_LINKER_FLAGS=$(Quote "${NCBI_COMPILER_EXE_LINKER_FLAGS}")"
fi

if [ -z "$BUILD_ROOT" ]; then
  if test "$CMAKE_GENERATOR" = "Xcode"; then
    BUILD_ROOT=CMake-${CC_NAME}${CC_VERSION}
    if test "$BUILD_SHARED_LIBS" = "ON"; then
      BUILD_ROOT="$BUILD_ROOT"-DLL
    fi
  else
    BUILD_ROOT=CMake-${CC_NAME}${CC_VERSION}-${BUILD_TYPE}
    if test "$BUILD_SHARED_LIBS" = "ON"; then
      BUILD_ROOT="$BUILD_ROOT"DLL
    fi
#BUILD_ROOT="$BUILD_ROOT"64
  fi
fi

cd ${tree_root}
Check_function_exists configure_ext_PreCMake && configure_ext_PreCMake
# true to debug
if false; then
  echo mkdir -p ${BUILD_ROOT}/build 
  echo Running "${CMAKE_CMD}" ${CMAKE_ARGS} "${tree_root}/src"
else
mkdir -p ${BUILD_ROOT}/build
if [ ! -d ${BUILD_ROOT}/build ]; then
  Error "Failed to create directory: ${BUILD_ROOT}/build"
fi
cd ${BUILD_ROOT}/build 

if [ -f "${tree_root}/CMakeLists.txt" ]; then
  eval "${CMAKE_CMD}" ${CMAKE_ARGS}  "${tree_root}"
else
  eval "${CMAKE_CMD}" ${CMAKE_ARGS}  "${tree_root}/src"
fi
fi
cd ${initial_dir}
