#!/bin/sh
#
# $Id$
# Set some "domestic" paths, then run "configure"
#
# By Denis Vakatov (vakatov@ncbi.nlm.nih.gov)
#############################################################################


# Parse and check the args and the working dir
#
post_usage=
USR_BUILD_DIR=
case "$1" in
  --help | -help | --h | -h | --usage | -usage) post_usage="yes";;
  -*) ;;
  *) if test $# -gt 0; then USR_BUILD_DIR="$1" ; shift ; fi ;;
esac
if test ! -f ./configure ; then
  post_usage="yes"
fi
if test "$post_usage" = "yes" ; then
  cat << EOF
USAGE: Go to the root c++ directory and run:
  ./compilers/`basename $0` [build_dir] [flags]

  build_dir           - name of the target build directory(e.g. just ".")
                        NOTE:  it should not start from '-'!
  flags (optional):
    --without-debug     - build the release versions of libs and apps
    --without-fastcgi   - explicitely prohibit the use of Fast-CGI library
    --without-internal  - do not build internal projects
EOF
  exit 1
fi


# Set "domestic" paths and flags
#
DEF_BUILD_DIR="egcs_sun"
set -a
CXXFLAGS="-fsquangle"
NCBI_C_INCLUDE=${NCBI_C_INCLUDE:="$NCBI/ver0.0/ncbi/include"}
NCBI_C_LIB=${NCBI_C_RELEASE_LIB:="$NCBI/ver0.0/ncbi/altlib"}
NCBI_C_DEBUG_LIB=${NCBI_C_DEBUG_LIB:="$NCBI/ver0.0/ncbi/altlib"}

THREAD_LIBS=${THREAD_LIBS:="-lthread -lpthread"}
NETWORK_LIBS=${NETWORK_LIBS:="-lsocket -lnsl"}
FASTCGI_LIBS=${FASTCGI_LIBS:="-lfcgi4.2"}

KeepStateTarget=".KEEP_STATE:"


# Run the CONFIGURE script
#
sh ./configure --exec_prefix="${USR_BUILD_DIR:=$DEF_BUILD_DIR}" "$@"
