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

add_gpipe_warnings()
{
  _ext_CFLAGS="${_ext_CFLAGS:-}${_ext_CFLAGS:+ }-Wmissing-prototypes"
  _ext_CXXFLAGS="${_ext_CXXFLAGS:-}${_ext_CXXFLAGS:+ }-Wnon-virtual-dtor -Wall -Wextra -Wconversion -Wdeprecated-declarations -Wlogical-op -Wmissing-declarations -Wpedantic -Wshadow -Wsuggest-attribute=format -Wswitch -Wpointer-arith -Wcast-align -Wmissing-include-dirs -Winvalid-pch -Wmissing-format-attribute"
}

configure_ext_Usage()
{
    cat <<EOF
GPipe Predefined Settings:
  --gpipe-prod            for production use (--with-dll --without-debug)
  --gpipe-dev             for development and debugging (--with-dll)
  --gpipe-cgi             for deployment of web CGIs (--without-debug)
  --gpipe-distrib         for external distribution to the public.
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
      BUILD_TYPE="Release"
      BUILD_SHARED_LIBS="ON"
      PROJECT_FEATURES="${PROJECT_FEATURES};Int8GI"
      PROJECT_COMPONENTS="${PROJECT_COMPONENTS};WGMLST"
      : "${BUILD_ROOT:=../Release}"
      add_gpipe_warnings
      ;; 
    "--gpipe-dev")
      BUILD_TYPE="Debug"
      BUILD_SHARED_LIBS="ON"
      PROJECT_FEATURES="${PROJECT_FEATURES};StrictGI"
      PROJECT_COMPONENTS="${PROJECT_COMPONENTS};WGMLST"
      : "${BUILD_ROOT:=../Debug}"
      add_gpipe_warnings
      ;; 
    "--gpipe-cgi")
      BUILD_TYPE="Release"
      BUILD_SHARED_LIBS="OFF"
      PROJECT_FEATURES="${PROJECT_FEATURES};Int8GI"
      PROJECT_COMPONENTS="${PROJECT_COMPONENTS};WGMLST"
      : "${BUILD_ROOT:=../Static}"
      add_gpipe_warnings
      ;; 
    "--gpipe-distrib")
      BUILD_TYPE="Release"
      BUILD_SHARED_LIBS="OFF"
      PROJECT_COMPONENTS="${PROJECT_COMPONENTS};WGMLST;-PCRE"
      : "${BUILD_ROOT:=../Distrib}"
      add_gpipe_warnings
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
  CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_C_FLAGS=$(Quote "${_ext_CFLAGS}") -DCMAKE_CXX_FLAGS=$(Quote "${_ext_CXXFLAGS}")"
}
