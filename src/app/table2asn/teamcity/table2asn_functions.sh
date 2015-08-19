#!/bin/bash

CONVERT_CREATE_DATE="
/^ +create-date std \{/b edit_year
/^ +update-date std \{/b edit_year
/^ +date std \{/b edit_year
b
:edit_year
n
/^ +year 201[456]/b edit_date
b
:edit_date
s/year [0-9]+/year 2014/
n
s/month [0-9]+/month 2/
n
s/day [0-9]+/day 1/
b
"

CONVERT_TOOL_VERSION="s/(    tool \\\"table2asn 1\.0\.).*/\10\\\"/"

function create_output_file()
{
   local args=$1
   local result=$2

   (cd test-cases && echo "$TEST_TOOL_PATH $args -r $tmp_folder -o - "| bash | sed -r \
      -e "$CONVERT_TOOL_VERSION" \
      -e "$CONVERT_CREATE_DATE" \
    > $result || true)
}

function make_test_name()
{
   echo "$1" | sed -r -e 's/^-i //' -e 's/[ &=\\.\\(\\),\:\/"\x27]+/-/g' -e 's/--/-/g'
}
