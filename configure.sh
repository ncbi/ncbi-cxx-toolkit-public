#!/bin/sh
#
# $Id$
# The configurable "configure" launcher
#
# By Denis Vakatov (vakatov@ncbi.nlm.nih.gov)
#############################################################################


#
# Parse and check the args and the working environment
#

post_usage=
USR_BUILD_DIR=

#  Are we running from the right directory?
if test ! -f ./configure;  then
  echo "Bad working directory, cannot find \"./configure\" script"
  post_usage="yes"
fi

#  Compiler to use (mandatory)
if test "$post_usage" != "yes"  &&  test -n "$1" ;  then
  compiler="$1"
  env_file="./compilers/${compiler}.env"
  shift
else
  post_usage="yes"
fi

#  Build directory (optional)
if test "$post_usage" != "yes" ;  then
  case "$1" in
    -*) ;;
    *) if test -n "$1" ;  then
         USR_BUILD_DIR="$1"
         shift
       fi ;;
  esac
fi

#  Environment settings for the chosen platform/compiler (mandatory)
if test "$post_usage" != "yes"  &&  test ! -f "$env_file" ;  then
  echo "Cannot find the environment file for compiler \"$compiler\"."
  echo "The compiler must be one of:"
  echo -n "  "
  ( cd ./compilers/ && echo *.env ) 2>/dev/null | sed 's/\.env//g'
  post_usage="yes"
fi


#
# Printout the USAGE, if necessary
#

if test "$post_usage" = "yes" ; then
  cat << EOF

USAGE: Go to the root c++ directory and run:
  ./`basename $0` compiler [build_dir] [flags]

  compiler             - platform/compiler to use; it must be registered in the
                         "compilers/" dir(basename of one of the "*.env" files)
  build_dir            - name of the target build directory(e.g. just "."); it
                         will be the value of <compiler> arg by default
                         NOTE:  it should not start from '-'!
  flags (optional):
    --without-debug    - build the release versions of libs and apps
    --without-fastcgi  - explicitely prohibit the use of Fast-CGI library
    --with-internal    - build internal projects (along with the public ones)
    --without-bincopy  - dont check whether the copied binaries are identical
EOF
  exit 1
fi


#
# Set the "domestic" environment:  paths and flags
#

set -a

#  Default build dir
DEF_BUILD_DIR="$compiler"

# Default shell to run the "configure" with
#CONFIG_SHELL=

#  Default flags for the "configure"
USR_CONF_FLAGS=

#  Import the platform/compiler specific environment
. $env_file
if test $? -ne 0 ; then
  echo "Error in executing \"$env_file\""
  exit 2
fi


#
# Run CONFIGURE
#

${CONFIG_SHELL-/bin/sh} ./configure --exec_prefix="${USR_BUILD_DIR:=$DEF_BUILD_DIR}" $USR_CONF_FLAGS "$@"
