#! /bin/sh

# $Id$
# Author:  Vladimir Ivanov, NCBI 
#
###########################################################################
#
# Compile a check script and copy necessary files to run tests in the 
# MS VisualC++ build tree.
#
# Usage:
#    check_make_xcode_cfg.sh <action> <solution> <static|dll> <cfg> [build_dir]
#
#    <action>      - { create } 
#                      create - create check script (this arg needs for compatibility).
#    <solution>    - solution file name without .sln extention
#                    (relative path from build directory).
#    <static|dll>  - type of used libraries (static, dll).
#    <cfg>         - configuration name
#                    (Debug, DebugDLL, DebugMT, Release, ReleaseDLL, ReleaseMT).
#    [build_dir]   - path to MSVC build tree like ".../msvc_prj"
#                    (default: will try determine path from current work
#                    directory -- root of build tree) 
#
###########################################################################


# Get script directory
script_dir=`dirname $0`
script_dir=`(cd "$script_dir"; pwd)`

# Run actual script
$script_dir/check_make_cfg.sh XCODE $*

exit $?
