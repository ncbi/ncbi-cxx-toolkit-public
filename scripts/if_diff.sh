#!/bin/sh
#################################
# $Id$
# Author:  Denis Vakatov (vakatov@ncbi.nlm.nih.gov)
#################################

script_name=`basename $0`
script_args="$*"

action="$1"
shift 1


Usage()
{
  cat << EOF
"$script_name $script_args"::  $1!

USAGE:   $script_name <action> <f1> <f2> ... <fN> <dest_dir>
         $script_name <action> <src_file> <dest_file>
Example: $script_name "cp -p" abc.o ../def.a /tmp
Synopsis:
   Execute "action f dest_dir/f" for all files "f1", "f2", ..., "fN" that are
   missing in "dest_dir" or different (in the sense of "cmp -s")
   from their existing counterparts in "dest_dir".
   If the 1st arg is a file and "dest_file" does not exist or if it is
   different from "src_file" then execute "action src_file dest_file".
EOF

  exit 1
}

ExecAction()
{
  src_file="$1"
  dest_file="$2"
  cmd="$action $src_file $dest_file"
  cmp -s "$src_file" "$dest_file"  ||
  ( echo "$cmd" ;  $cmd )  ||
  Usage "\"$cmd\" failed"
}


test $# -gt 1  ||  Usage "too few cmd.-line parameters"

args=
i=0
for f in $* ; do
  i=$[i+1]
  if test $i -eq $# ; then
    dest="$f"
  else
    args="$args $f"  
  fi
done

if test ! -d $dest ; then
  test -f $1  ||
    Usage "the destination is not a directory, and the source is not a file"
  test $# -lt 3  ||
    Usage "the destination is not a directory, and too many arguments"
  ExecAction "$1" $dest
  exit 0
fi


test -d $dest  ||  Usage "destination is neither file nor directory"

i=2
for f in $args ; do
  i=$[i+1]
  test -f $f  ||  Usage "\"$f\"(field #$i) is not a file"
done

for f in $args ; do
  ExecAction $f "$dest/`basename $f`"
done

exit 0
