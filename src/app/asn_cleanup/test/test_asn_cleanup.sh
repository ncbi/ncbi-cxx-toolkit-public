#!/bin/bash

set -o errexit
set -o pipefail
set -x

TEST_TOOL_NAME="$1"
TEST_TOOL_PATH="$2"
tmp_folder="$3"
diff_folder="$4"
debug="$5"

if [ -z "$TEST_TOOL_NAME" -o -z "$TEST_TOOL_PATH" -o -z "$tmp_folder" -o -z "$diff_folder" ]
then
    echo "usage: $0 <tool-name> <binary-path> <tmp-directory>i <diff-folder>"
    exit 1
fi

if [ -z $debug ];then
  debug="no"
  savediff=""
fi

if [ "$debug" == "savediff" ];then
  savediff="yes"
fi

self_script="$0"

if [ -z $diff_folder ]; then
   diff_folder=$tmp_folder/diff
fi

script_folder=${self_script%/*}
if [ -z "$script_folder" ]; then
   script_folder=`pwd`
fi

. $script_folder/common_functions.sh
. $script_folder/${TEST_TOOL_NAME}_functions.sh

input_folder=$script_folder

mkdir -p "$tmp_folder"
mkdir -p "$diff_folder"

unit_test_compare_to_previous . test_cases
#unit_test_compare_to_old_tool test-cases test-case1
