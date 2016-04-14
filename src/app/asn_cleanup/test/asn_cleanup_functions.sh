#!/bin/bash

OLD_TOOL_PATH=asnval

function create_output_file()
{
   local args=$1
   local result=$2
   local cerr=$3

   (echo "$TEST_TOOL_PATH $args -Q 0 -o $result "| bash  \
    > $cerr  2>&1 && echo "end-of-file" >> $result ) || echo "$TEST_TOOL_PATH" "returned non-zero exit code"
}

function make_test_name()
{
   echo "$1" | sed -r -e 's/^-i //' -e 's/[ &=\\.\\(\\),\:\/"]+/-/g' -e 's/--/-/g'
}

function run_old_tool()
{ 
   local args=$1
   local result=$2
   local cerr=$3
   (echo "$OLD_TOOL_PATH $args -Q 0 -o $result "| bash  \
    > $cerr  2>&1 && echo "end-of-file" >> $result ) || echo "$OLD_TOOL_PATH" "returned non-zero exit code"
}
