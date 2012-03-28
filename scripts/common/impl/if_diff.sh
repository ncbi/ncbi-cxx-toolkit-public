#! /bin/sh
#################################
# $Id$
# Author:  Denis Vakatov (vakatov@ncbi.nlm.nih.gov)
#################################

script_name=`basename $0`
script_args="$*"

action="$1"
shift 1

orig_PATH=$PATH
PATH=/bin:/usr/bin

case "`basename \"$action\"`" in
  cp | cp\ * | ln | ln\ * ) rm="rm -f" ;;
  * ) rm=: ;;
esac

if test "$1" = "-q" ; then
  quiet="yes"
  shift 1
else
  quiet="no"
fi

Usage()
{
  fmt -s -w 79 << EOF
USAGE:   $script_name <action> [-q] <f1> <f2> ... <fN> <dest_dir>
         $script_name <action> [-q] <src_file> <dest_file>
EXAMPLE: $script_name "cp -p" abc.o ../def.a /tmp
SYNOPSIS:
   Execute "action f dest_dir/f" for all files "f1", "f2", ..., "fN" that are
   missing in "dest_dir" or different (in the sense of "cmp -s")
   from their existing counterparts in "dest_dir".
   If the 1st arg is a file and "dest_file" does not exist or if it is
   different from "src_file" then execute "action src_file dest_file".
   [-q] -- the "smart quiet" optional flag: ignore error if no files specified.

ERROR: "$script_name $script_args"::  $1!
EOF

  exit 1
}

ExecHelper()
{
  dest_file=$1
  $rm "$dest_file"
  shift
  cmd="$* $dest_file"
  test "$quiet" = yes || echo "$cmd"
  PATH=$orig_PATH
  "$@" "$dest"
  status=$?
  PATH=/bin:/usr/bin
  return $status
}

ExecAction()
{
  src_file="$1"
  dest_file="$2"
  cmp -s "$src_file" "$dest_file"  ||
  ExecHelper "$dest_file" $action "$src_file"  ||
  case "`basename \"$action\"`" in
    ln | ln\ -f )
      test "$quiet" = yes || echo "failed; trying \"cp -p ...\" instead"
      cmd="cp -p $src_file $dest_file"
      ExecHelper "$dest_file" cp -p "$src_file"  ||
      Usage "\"$cmd\" failed"
      ;;
    *) Usage "\"$cmd\" failed" ;;
  esac
}


test $# -lt 1  &&  Usage "too few command-line parameters"
if test $# -lt 2 ; then
  if test "$quiet" = "yes"  &&  test -d "$1" ; then
    exit 0
  fi
  Usage "too few command-line parameters"
fi


for f in "$@" ; do
  dest=$f
done

if test ! -d "$dest" ; then
  err_base="the destination ($dest) is not a directory, but"
  test $# -lt 3  ||  Usage "$err_base multiple sources were specified"
  test -f "$1"   ||  Usage "$err_base the source ($1) is absent or not a file"
  ExecAction "$1" "$dest"
  exit 0
fi

i=1
for f in "$@" ; do
  test $i -eq $# -o -f "$f"  ||  Usage "source $i ($f) is absent or not a file"
  i=`expr $i + 1`  
done

i=1
for f in "$@" ; do
  test $i -ge $#  ||  ExecAction $f "$dest/`basename $f`"
  i=`expr $i + 1`  
done

exit 0
