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
#    check_make_win_cfg.sh <action> <solution> <static|dll> <cfg> [build_dir]
#
#    <action>      - { init | create } 
#                      init   - initialize master script directory (MSVC only);
#                      create - create check script.
#    <solution>    - solution file name without .sln extention
#                    (relative path from build directory).
#    <static|dll>  - type of used libraries (static, dll).
#    <cfg>         - configuration name
#                    (DebugDLL, DebugMT, ReleaseDLL, ReleaseMT, Unicode_*).
#    [build_dir]   - path to MSVC build tree like ".../msvc_prj"
#                    (default: will try determine path from current work
#                    directory -- root of build tree) 
#
###########################################################################


# Get script directory
script_dir=`dirname $0`
script_dir=`(cd "$script_dir"; pwd)`

# Run actual script
$script_dir/check_make_cfg.sh MSVC $*

exit $?
