#! /bin/sh
#
# $Id$
#

i=0
j=`expr $$ % 4 + 1`
while [ $i -lt $j ]; do
  test_fstream_pushback  ||  exit
  i="`expr $i + 1`"
done
