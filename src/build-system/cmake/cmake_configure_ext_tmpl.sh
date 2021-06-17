#!/bin/sh

#############################################################################
# $Id$
#   Template of a CMake configuration extension script
#   Author: Andrei Gourianov, gouriano@ncbi
#
#   Usage:
#      Based on this template, create an extension script for your team
#      call it cmake_configure_ext_team.sh, for example.
#      In the root directory of your build tree create cmake_configure_ext.sh
#      with the following contents:
#           source `dirname $0`/cmake_configure_ext_team.sh
#      This script will be included into cmake-cfg-unix.sh or cmake-cfg-xcode.sh
#
#   Note:
#      Be careful not to overwrite parent script variables accidentally.
#
#############################################################################

# Verify that script is not being run standalone
_ext_check=`type -t Check_function_exists`
test "${_ext_check}" = "function"
if [ $? -ne 0 ]; then
  echo This is an extension script. It cannot be run standalone.
  echo Instead, it should be included into another script.
  exit 1
fi


# Print information about additional settings
configure_ext_Usage()
{
    cat <<EOF
Additional settings:
  --a                     -- option a
  --b                     -- option b
EOF
}


# Parse command line
# First argument of the function is the name of the variable which receives
# arguments which were not recognized by this function.
# Other arguments are those which were not recognized by the parent script.
#
# The function is called after the parent script has parsed arguments which it understands
# and receives unrecognized arguments only.
configure_ext_ParseArgs()
{
# unrecognized arguments will go into this variable
  local _ext_retval=$1
  shift
# unrecognized arguments will be collected here
  local _ext_unknown=""

#parse arguments, modify parent script variables if needed
  while [ $# != 0 ]; do
    case "$1" in 
    --a)
      echo option a received
      ;; 
    --b)
      echo option b received
      ;; 
    *) 
# collect unknown arguments
      _ext_unknown="${_ext_unknown} $1"
      ;; 
    esac 
    shift 
  done 
# put unrecognized arguments into return variable
  eval "${_ext_retval}='${_ext_unknown}'"
}


# The function is called right before running CMake configuration.
# Add more parameters into CMAKE_ARGS if needed
configure_ext_PreCMake()
{
  CMAKE_ARGS="$CMAKE_ARGS --trace"
}
