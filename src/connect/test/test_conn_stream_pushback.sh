#! /bin/sh
#
# $Id$
#

i=0
while [ $i -lt 5 ]; do
  ./test_conn_stream_pushback  ||  exit
  i="`expr $i + 1`"
done
