#! /bin/sh
#
# $Id$
#

i=0
j=`expr $$ % 2 + 1`
while [ $i -lt $j ]; do
  date
  $CHECK_EXEC test_conn_stream_pushback  ||  exit
  i="`expr $i + 1`"
done
date
