#! /bin/sh

# $Id$
# Author:  Dmitriy Beloslyudtsev, NCBI 
#
#  Filter out redundant warnings issued by Intel C++ (ICC) compiler.

tempfile="tmp$$"

"$@" 2>$tempfile

ret_code="$?"

cat $tempfile |
grep -v '^icc: NOTE: The evaluation period for this product ends on' |
grep -v '^conftest\.C:$' 1>&2

rm $tempfile

exit $ret_code
