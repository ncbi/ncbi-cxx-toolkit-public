#!/bin/sh

#############################################################################
# $Id$
#############################################################################

_ext_check=`type -t Check_function_exists`
test "${_ext_check}" = "function"
if [ $? -ne 0 ]; then
  echo This is an extension script. It cannot be run standalone.
  echo Instead, it should be included into another script.
  exit 1
fi

configure_common_gpipe()
{
  PROJECT_LIST=${tree_root}/scripts/projects/ncbi_gpipe.lst
  _ext_EXE_LINKER_FLAGS="${_ext_EXE_LINKER_FLAGS:-}"
  _ext_CFLAGS="${_ext_CFLAGS:-}${_ext_CFLAGS:+ }-Wmissing-prototypes"
  _ext_CXXFLAGS="${_ext_CXXFLAGS:-}${_ext_CXXFLAGS:+ }-Wnon-virtual-dtor -Wall -Wextra -Wconversion -Wdeprecated-declarations -Wlogical-op -Wmissing-declarations -Wpedantic -Wshadow -Wsuggest-attribute=format -Wswitch -Wpointer-arith -Wcast-align -Wmissing-include-dirs -Winvalid-pch -Wmissing-format-attribute"
}

configure_ext_Usage()
{
    cat <<EOF

GPIPE OPTIONS

  GPipe compilations should use one of these 4 predefined settings:

  --gpipe-prod            for production use (--with-dll --without-debug)
  --gpipe-dev             for development and debugging (--with-dll)
  --gpipe-cgi             for deployment of web CGIs (--without-debug)
  --gpipe-distrib         for external distribution to the public

  For focused debugging, trading performance for maximum safety:

  --gpipe-mem-debug       for debug, sanitizers 
  --gpipe-max-debug       for debug, STL, sanitizers (excludes apps using WGMLST)

  For debugging/patching in production via drop-in app replacement (mostly static):

  --gpipe-prod-patch      for patching with Release (not dll); same as --gpipe-cgi
  --gpipe-prod-debug      for general debugging but as drop-in (not dll)
  --gpipe-prod-mem-check  for memory leaks/bugs (not dll; with sanitizers)
  --gpipe-prod-mem-debug  for memory leaks/bugs (not dll; debug with sanitizers)
  --gpipe-prod-max-debug  for limited cases given slow performance (not dll; max debug)

  For special cases:

  --gpipe-deps            for dependencies only (e.g. Toolkit) without internal/gpipe

  NOTE: GPipe settings override several Toolkit defaults, such as
        compilation warnings, --with-components, --with-features.

EOF
}

configure_ext_ParseArgs()
{
  local _ext_retval=$1
  shift
  local _ext_unknown=""
  while [ $# != 0 ]; do
    case "$1" in 
    "--gpipe-prod")
      GPIPE_MODE=prod
      BUILD_TYPE="Release"
      BUILD_SHARED_LIBS="ON"
      PROJECT_FEATURES="${PROJECT_FEATURES};Int8GI"
      PROJECT_COMPONENTS="${PROJECT_COMPONENTS};WGMLST"
      : "${BUILD_ROOT:=../Release}"
      configure_common_gpipe
      _ext_EXE_LINKER_FLAGS="${_ext_EXE_LINKER_FLAGS:-}${_ext_EXE_LINKER_FLAGS:+ }-lSegFault"
      ;; 
    "--gpipe-dev")
      GPIPE_MODE=dev
      BUILD_TYPE="Debug"
      BUILD_SHARED_LIBS="ON"
      PROJECT_FEATURES="${PROJECT_FEATURES};StrictGI;StrictEntrezId"
      PROJECT_COMPONENTS="${PROJECT_COMPONENTS};WGMLST"
      : "${BUILD_ROOT:=../Debug}"
      configure_common_gpipe
      _ext_EXE_LINKER_FLAGS="${_ext_EXE_LINKER_FLAGS:-}${_ext_EXE_LINKER_FLAGS:+ }-lSegFault"
      ;;
    "--gpipe-cgi")
      GPIPE_MODE=cgi
      BUILD_TYPE="Release"
      BUILD_SHARED_LIBS="OFF"
      PROJECT_FEATURES="${PROJECT_FEATURES};Int8GI"
      PROJECT_COMPONENTS="${PROJECT_COMPONENTS};WGMLST"
      : "${BUILD_ROOT:=../Static}"
      configure_common_gpipe
      ;; 
    "--gpipe-distrib")
      GPIPE_MODE=distrib
      BUILD_TYPE="Release"
      BUILD_SHARED_LIBS="OFF"
      PROJECT_FEATURES="${PROJECT_FEATURES};Int8GI"
      PROJECT_COMPONENTS="${PROJECT_COMPONENTS};WGMLST;-PCRE"
      : "${BUILD_ROOT:=../Distrib}"
      configure_common_gpipe
      ;;

    "--gpipe-mem-debug")
      GPIPE_MODE=mem-debug
      BUILD_TYPE="Debug"
      BUILD_SHARED_LIBS="ON"
      PROJECT_FEATURES="${PROJECT_FEATURES};Int8GI"
      PROJECT_COMPONENTS="${PROJECT_COMPONENTS};WGMLST"
      : "${BUILD_ROOT:=../MemDebug}"
      configure_common_gpipe
      _ext_EXE_LINKER_FLAGS="${_ext_EXE_LINKER_FLAGS:-}${_ext_EXE_LINKER_FLAGS:+ }-lSegFault -fsanitize=address"
      _ext_CFLAGS="$_ext_CFLAGS -fsanitize=address -DNCBI_USE_LSAN"
      _ext_CXXFLAGS="$_ext_CXXFLAGS -fsanitize=address -DNCBI_USE_LSAN"
      ;;
    "--gpipe-max-debug")
      GPIPE_MODE=max-debug
      BUILD_TYPE="Debug"
      BUILD_SHARED_LIBS="ON"
      PROJECT_FEATURES="${PROJECT_FEATURES};Int8GI;MaxDebug"
      PROJECT_COMPONENTS="${PROJECT_COMPONENTS}" # WGMLST doesn't support MaxDebug yet.
      : "${BUILD_ROOT:=../MaxDebug}"
      configure_common_gpipe
      _ext_EXE_LINKER_FLAGS="${_ext_EXE_LINKER_FLAGS:-}${_ext_EXE_LINKER_FLAGS:+ }-lSegFault -fsanitize=address"
      _ext_CFLAGS="$_ext_CFLAGS -fsanitize=address -DNCBI_USE_LSAN"
      _ext_CXXFLAGS="$_ext_CXXFLAGS -fsanitize=address -DNCBI_USE_LSAN"
      ;;

    "--gpipe-prod-patch")
      GPIPE_MODE=prod-patch
      BUILD_TYPE="Release"
      BUILD_SHARED_LIBS="OFF"
      PROJECT_FEATURES="${PROJECT_FEATURES};Int8GI"
      PROJECT_COMPONENTS="${PROJECT_COMPONENTS};WGMLST"
      : "${BUILD_ROOT:=../Static}"
      configure_common_gpipe
      ;; 
    "--gpipe-prod-debug")
      GPIPE_MODE=prod-debug
      BUILD_TYPE="Debug"
      BUILD_SHARED_LIBS="OFF"
      PROJECT_FEATURES="${PROJECT_FEATURES};Int8GI"
      PROJECT_COMPONENTS="${PROJECT_COMPONENTS};WGMLST"
      : "${BUILD_ROOT:=../DebugStatic}"
      configure_common_gpipe
      _ext_EXE_LINKER_FLAGS="${_ext_EXE_LINKER_FLAGS:-}${_ext_EXE_LINKER_FLAGS:+ }-lSegFault"
      ;;
    "--gpipe-prod-mem-check")
      GPIPE_MODE=prod-mem-check
      BUILD_TYPE="Release"
      BUILD_SHARED_LIBS="OFF"
      PROJECT_FEATURES="${PROJECT_FEATURES};Int8GI"
      PROJECT_COMPONENTS="${PROJECT_COMPONENTS};WGMLST"
      : "${BUILD_ROOT:=../MemCheckStatic}"
      configure_common_gpipe
      _ext_EXE_LINKER_FLAGS="${_ext_EXE_LINKER_FLAGS:-}${_ext_EXE_LINKER_FLAGS:+ }-lSegFault -fsanitize=address"
      _ext_CFLAGS="$_ext_CFLAGS -fsanitize=address -DNCBI_USE_LSAN"
      _ext_CXXFLAGS="$_ext_CXXFLAGS -fsanitize=address -DNCBI_USE_LSAN"
      ;;
    "--gpipe-prod-mem-debug")
      GPIPE_MODE=prod-mem-debug
      BUILD_TYPE="Debug"
      BUILD_SHARED_LIBS="OFF"
      PROJECT_FEATURES="${PROJECT_FEATURES};Int8GI"
      PROJECT_COMPONENTS="${PROJECT_COMPONENTS};WGMLST"
      : "${BUILD_ROOT:=../MemDebugStatic}"
      configure_common_gpipe
      _ext_EXE_LINKER_FLAGS="${_ext_EXE_LINKER_FLAGS:-}${_ext_EXE_LINKER_FLAGS:+ }-lSegFault -fsanitize=address"
      _ext_CFLAGS="$_ext_CFLAGS -fsanitize=address -DNCBI_USE_LSAN"
      _ext_CXXFLAGS="$_ext_CXXFLAGS -fsanitize=address -DNCBI_USE_LSAN"
      ;;
     "--gpipe-prod-max-debug")
      GPIPE_MODE=prod-max-debug
      BUILD_TYPE="Debug"
      BUILD_SHARED_LIBS="OFF"
      PROJECT_FEATURES="${PROJECT_FEATURES};Int8GI;MaxDebug"
      PROJECT_COMPONENTS="${PROJECT_COMPONENTS}" # WGMLST doesn't support MaxDebug yet.
      : "${BUILD_ROOT:=../MaxDebugStatic}"
      configure_common_gpipe
      _ext_EXE_LINKER_FLAGS="${_ext_EXE_LINKER_FLAGS:-}${_ext_EXE_LINKER_FLAGS:+ }-lSegFault -fsanitize=address"
      _ext_CFLAGS="$_ext_CFLAGS -fsanitize=address -DNCBI_USE_LSAN"
      _ext_CXXFLAGS="$_ext_CXXFLAGS -fsanitize=address -DNCBI_USE_LSAN"
      ;;

    "--gpipe-deps")
      GPIPE_MODE=prod
      BUILD_TYPE="Release"
      BUILD_SHARED_LIBS="ON"
      PROJECT_FEATURES="${PROJECT_FEATURES};Int8GI"
      PROJECT_COMPONENTS="${PROJECT_COMPONENTS};WGMLST"
      : "${BUILD_ROOT:=../Release}"
      configure_common_gpipe
      PROJECT_LIST=${tree_root}/scripts/projects/ncbi_gpipe_deps.lst
      _ext_EXE_LINKER_FLAGS="${_ext_EXE_LINKER_FLAGS:-}${_ext_EXE_LINKER_FLAGS:+ }-lSegFault"
      ;;

    *) 
      _ext_unknown="${_ext_unknown} $1"
      ;; 
    esac 
    shift 
  done 
  eval "${_ext_retval}='${_ext_unknown}'"
}

configure_ext_PreCMake()
{
  case "${GPIPE_MODE:-}" in
  "") echo "WARNING: Configuring without choosing a predefined GPipe setting."
      configure_ext_Usage
      ;;
  esac

  CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_C_FLAGS=$(Quote "${_ext_CFLAGS}") -DCMAKE_CXX_FLAGS=$(Quote "${_ext_CXXFLAGS}") -DCMAKE_EXE_LINKER_FLAGS=$(Quote "${_ext_EXE_LINKER_FLAGS}") -DNCBI_ThirdParty_GRPC=$NCBI/grpc-1.67.1-ncbi1 -DNCBI_ThirdParty_PROTOBUF=$NCBI/grpc-1.67.1-ncbi1"
}
