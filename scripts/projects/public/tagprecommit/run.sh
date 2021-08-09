#!/usr/bin/env bash

script_dir=`dirname $0`
script_dir=`(cd "${script_dir}" ; pwd)`

$script_dir/gen_all.py
$script_dir/use_embedded_ptb.py
