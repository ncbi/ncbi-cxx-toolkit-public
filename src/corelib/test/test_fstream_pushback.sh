#! /bin/sh
#
# $Id$
#

i=0
while [ $i -lt 5 ]; do
  ./test_fstream_pushback  ||  exit
  i="`expr $i + 1`"
done
