#!/bin/bash

set -o errexit
set -o pipefail
#set -x

table2asn="$1"
tmp_folder="$2"
diff_folder="$3"
debug="$4"

if [ -z "$table2asn" -o -z "$tmp_folder" -o -z "$diff_folder" ]
then
    echo "usage: $0 <binary-path> <tmp-directory>i <diff-folder>"
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
input_folder=$script_folder/test-cases

mkdir -p "$tmp_folder"
mkdir -p "$diff_folder"

unit_test_compare_to_previous test-cases test-case1
