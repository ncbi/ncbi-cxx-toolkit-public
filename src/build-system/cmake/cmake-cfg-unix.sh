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
script_root=`(cd "${script_dir}/../../.." ; pwd)`
script_args="$@"
tree_root=`pwd`
extension="cmake_configure_ext.sh"
prebuilds=""

host_os=`uname`
if test -z "${CMAKE_CMD}" -a $host_os = "Darwin"; then
  CMAKE_CMD=`which cmake 2>/dev/null`
  if test $? -ne 0; then
    for i in /usr/local/bin/cmake /opt/homebrew/bin/cmake \
             /Applications/CMake.app/Contents/bin/cmake /sw/bin/cmake
    do
      if test -x $i  &&  $i --version >/dev/null 2>&1; then
        CMAKE_CMD=$i
        break
      fi    
    done
  fi
fi
if [ -z "${CMAKE_CMD}" ]; then
  CMAKE_CMD=`which cmake 2>/dev/null`
  if test $? -ne 0; then
    echo ERROR: CMake is not found 1>&2
    exit 1
  fi
fi
echo CMake: ${CMAKE_CMD}
${CMAKE_CMD} --version

############################################################################# 
# defaults
if [ -z "$CMAKECFGRECURSIONGUARD" ]; then
  BUILD_TYPE="Debug"
  BUILD_SHARED_LIBS="OFF"
  ALLOW_COMPOSITE="OFF"
  USE_CCACHE="OFF"
  USE_DISTCC="ON"
  SKIP_ANALYSIS="OFF"
  generator_multi_cfg=""
fi

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
  https://ncbi.github.io/cxx-toolkit/pages/ch_cmconfig#ch_cmconfig._Configure
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
  --with-composite-dll       -- same as "--with-dll" plus assemble composite shared libraries
  --with-composite           -- assemble composite shared or static libraries
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
  --with-ccache              -- use ccache
  --without-ccache           -- do not use ccache
  --with-distcc              -- use distcc
  --without-distcc           -- do not use distcc
  --without-analysis         -- skip source tree analysis
  --with-generator="X"       -- use generator X
  --with-conan               -- use Conan to install required components
OPTIONAL ENVIRONMENT VARIABLES:
  CMAKE_CMD                  -- full path to 'cmake'
  CMAKE_ARGS                 -- additional arguments to pass to 'cmake'
  CC or CMAKE_C_COMPILER     -- full path to C compiler
  CXX or CMAKE_CXX_COMPILER  -- full path to C++ compiler
EOF
  if [ -n "$prebuilds" ]; then
    echo "  --with-prebuilt=CFG        -- use build settings of an existing build"
    echo "             CFG is one of:"
    for d in $prebuilds; do
        echo "                                $d"
	done
  fi

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

Get_prebuild_path()
{
  for d in $prebuilds; do
    build_def=$d
    break
  done
  all=`echo $prebuilds | tr ' ' '\n'`
  while true; do
    cat <<EOF

Please pick a build configuration
Available:

$all

EOF
    read -p "Desired configuration (default = ${build_def}): " build_config
    test -n "$build_config"  ||  build_config=${build_def}
    prebuilt_path=${script_root}/${build_config}
    if [ -d ${prebuilt_path} ]; then
      break
    fi
  done
}

############################################################################# 
# parse arguments

do_help="no"
cxx_name=""
cxx_version=""
unknown=""
have_toolchain=""
dest=""

for arg in ${script_args}
do
  if test "$dest" = "CMAKE_GENERATOR"; then
    case "$arg" in 
       --*)
         dest=""
         ;;
      *) 
        CMAKE_GENERATOR="$CMAKE_GENERATOR $arg"
        continue
        ;; 
    esac
  fi
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
    --with-composite-dll) 
      BUILD_SHARED_LIBS=ON 
      ALLOW_COMPOSITE=ON
      ;; 
    --with-composite) 
      ALLOW_COMPOSITE=ON
      ;; 
    --with-debug) 
      BUILD_TYPE=Debug 
      ;; 
    --without-debug) 
      BUILD_TYPE=Release 
      ;; 
    --with-max-debug)
      BUILD_TYPE=Debug 
      if [ -z "$PROJECT_FEATURES" ]; then PROJECT_FEATURES="MaxDebug"; else PROJECT_FEATURES="$PROJECT_FEATURES;MaxDebug"; fi
      ;; 
    --with-symbols)
      if [ -z "$PROJECT_FEATURES" ]; then PROJECT_FEATURES="Symbols"; else PROJECT_FEATURES="$PROJECT_FEATURES;Symbols"; fi
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
      dest="CMAKE_GENERATOR"
      ;; 
    --with-components=*)
      PROJECT_COMPONENTS=${arg#*=}
      ;; 
    --with-features=*)
      if [ -z "$PROJECT_FEATURES" ]; then PROJECT_FEATURES="${arg#*=}"; else PROJECT_FEATURES="$PROJECT_FEATURES;${arg#*=}"; fi
      ;; 
    --with-build-root=*)
      BUILD_ROOT=${arg#*=}
      ;; 
    --with-prebuilt=*)
      prebuilt_path=${arg#*=}
      ;; 
    --with-conan)
      WITH_CONAN="ON"
      ;;
    [A-Z]*)
      cxx_name=$arg
      ;; 
    [1-9]*)
      cxx_version=$arg
      ;; 
    -DCMAKE_TOOLCHAIN_FILE* )
      CMAKE_ARGS="$CMAKE_ARGS $arg"
      have_toolchain="yes"
      ;; 
    -D* | --debug-* | --log-* | --trace* )
      CMAKE_ARGS="$CMAKE_ARGS $arg"
      ;; 
    *) 
      unknown="$unknown $arg"
      ;; 
  esac 
done 

if [ -n "${CMAKE_TOOLCHAIN_FILE}" ]; then
    have_toolchain="yes"
fi
if [ -n "$have_toolchain" -a -z "$BUILD_ROOT" ]; then
  Error "When supplying toolchain, please also specify build root (--with-build-root=...)" 
fi

tmp=`ls ${script_root}`
if [ -n "$tmp" ]; then
    for d in $tmp; do
        if [ -f ${script_root}/$d/cmake/buildinfo ]; then
            prebuilds="${prebuilds} $d"
        fi
	done
fi

if [ -n "$prebuilt_path" ]; then
    d=${script_root}/${prebuilt_path}
    if [ -d $d ]; then
        prebuilt_path=$d
    else
        Get_prebuild_path
    fi
    prebuilt_dir=`dirname $prebuilt_path`
    prebuilt_name=`basename $prebuilt_path`
fi

if [ -f $tree_root/$extension ]; then
  source $tree_root/$extension
fi

if [ $do_help = "yes" ]; then
  if [ "$cxx_name" = "Xcode" ]; then
    exec $script_dir/cmake-cfg-xcode.sh  "$@"
    exit 0
  else
    Usage
    exit 0
  fi
fi

if [ -z "$CMAKECFGRECURSIONGUARD" -a -n "$cxx_name" -a -z "$have_toolchain" ]; then
  if [ "$cxx_name" = "Xcode" ]; then
    exec $script_dir/cmake-cfg-xcode.sh  "$@"
    exit $?
  elif [ -x "$script_dir/cm${cxx_name}.sh" ]; then
    USE_DISTCC="OFF"
    export BUILD_TYPE BUILD_SHARED_LIBS ALLOW_COMPOSITE USE_CCACHE  USE_DISTCC  SKIP_ANALYSIS
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
    test -n "$CMAKE_C_COMPILER" && CC="$CMAKE_C_COMPILER"
    test -n "$CMAKE_CXX_COMPILER" && CXX="$CMAKE_CXX_COMPILER"
    test -n "$CMAKE_BUILD_TYPE" && BUILD_TYPE=$CMAKE_BUILD_TYPE
  else
    Error "Buildinfo not found in $prebuilt_dir/$prebuilt_name"
  fi
fi

############################################################################# 
if [ -z "$have_toolchain" ]; then
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
    if test $host_os = "Darwin"; then
      CC=`which clang 2>/dev/null`
    else
      CC=`which gcc 2>/dev/null`
    fi
    if test $? -ne 0; then
      CC=`which cc 2>/dev/null`
      if test $? -ne 0; then
        CC=""
      fi
    fi
  fi
  if [ -z "$CXX" ]; then
    if test $host_os = "Darwin"; then
      CXX=`which clang++ 2>/dev/null`
    else
      CXX=`which g++ 2>/dev/null`
    fi
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
    icx|icpx)
      CC_NAME="ICC"
      CC_VERSION=`$CC --version | awk 'NR==1{print $5}' | sed 's/[.]//g'`
      CC_VERSION=${CC_VERSION: -4}
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
fi

############################################################################# 
if test "$CMAKE_GENERATOR" = "Ninja Multi-Config"; then
  generator_multi_cfg="yes"
fi
if [ -z "$BUILD_ROOT" ]; then
  if test "$CMAKE_GENERATOR" = "Xcode"; then
    BUILD_ROOT=CMake-${CC_NAME}${CC_VERSION}
    if test "$BUILD_SHARED_LIBS" = "ON"; then
      BUILD_ROOT="$BUILD_ROOT"-DLL
    fi
  else
    if test "$generator_multi_cfg" = "yes"; then
      BUILD_ROOT=CMake-${CC_NAME}${CC_VERSION}
      if test "$BUILD_SHARED_LIBS" = "ON"; then
        BUILD_ROOT="$BUILD_ROOT"-DLL
      fi
      BUILD_TYPE=""
    else
      BUILD_ROOT=CMake-${CC_NAME}${CC_VERSION}-${BUILD_TYPE}
      if test "$BUILD_SHARED_LIBS" = "ON"; then
        BUILD_ROOT="$BUILD_ROOT"DLL
      fi
    fi
#BUILD_ROOT="$BUILD_ROOT"64
  fi
fi

cd ${tree_root}
mkdir -p ${BUILD_ROOT}/build
if [ ! -d ${BUILD_ROOT}/build ]; then
  Error "Failed to create directory: ${BUILD_ROOT}/build"
  exit 1
fi

############################################################################# 
if [ -z "$have_toolchain" ]; then
if [ -z "$NCBI_TOOLCHAIN" ]; then
  export CC CXX
  NCBI_TOOLCHAIN=`exec ${script_dir}/toolchains/cmkTool.sh "${CC_NAME}" "${CC_VERSION}"  "$@"`
  result=$?
  if test $result -ne 0; then
    NCBI_TOOLCHAIN=""
  fi
fi
if [ -r "$NCBI_TOOLCHAIN" ]; then
  tcpath=${BUILD_ROOT}/build/ncbi_toolchain.cmake
  if test "${tcpath}" = "${tcpath#/}" ; then
    tcpath=${tree_root}/${BUILD_ROOT}/build/ncbi_toolchain.cmake
  fi
  mv -f $NCBI_TOOLCHAIN ${tcpath}
  CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_TOOLCHAIN_FILE=$(Quote "${tcpath}")"
else
  if [ -n "$CC" ]; then
    CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_C_COMPILER=$(Quote "$CC")"
  fi
  if [ -n "$CXX" ]; then
    CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_CXX_COMPILER=$(Quote "$CXX")"
  fi

#CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY"
  if [ -n "$NCBI_COMPILER_EXE_LINKER_FLAGS" ]; then
    CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_EXE_LINKER_FLAGS=$(Quote "${NCBI_COMPILER_EXE_LINKER_FLAGS}")"
  fi
fi
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
if [ -n "$WITH_CONAN" ]; then
  CMAKE_ARGS="$CMAKE_ARGS -DNCBI_PTBCFG_USECONAN=$(Quote "${WITH_CONAN}")"
fi
if [ -n "$INSTALL_PATH" ]; then
  CMAKE_ARGS="$CMAKE_ARGS  -DNCBI_PTBCFG_INSTALL_PATH=$(Quote "${INSTALL_PATH}")"
fi
if [ -n "$BUILD_TYPE" ]; then
  CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_BUILD_TYPE=${BUILD_TYPE}"
fi
CMAKE_ARGS="$CMAKE_ARGS -DBUILD_SHARED_LIBS=$BUILD_SHARED_LIBS -DNCBI_PTBCFG_ALLOW_COMPOSITE=$ALLOW_COMPOSITE"
if test "$CMAKE_GENERATOR" != "Xcode"; then
  CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_USE_CCACHE=$USE_CCACHE"
  CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_USE_DISTCC=$USE_DISTCC"
fi

cd ${tree_root}
Check_function_exists configure_ext_PreCMake && configure_ext_PreCMake
# true to debug
if false; then
  echo Running "${CMAKE_CMD}" ${CMAKE_ARGS} "${tree_root}/src"
else
cd ${BUILD_ROOT}/build 

if [ -f "${tree_root}/CMakeLists.txt" ]; then
  eval "${CMAKE_CMD}" ${CMAKE_ARGS}  "${tree_root}"
else
  eval "${CMAKE_CMD}" ${CMAKE_ARGS}  "${tree_root}/src"
fi
fi
if test $? -ne 0; then
    exit 1
fi
cd ${initial_dir}
