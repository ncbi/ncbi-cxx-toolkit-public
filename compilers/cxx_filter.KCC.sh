#! /bin/sh

# $Id$
# Author:  Denis Vakatov, NCBI 
#
#  Filter out redundant warnings issued by KAI C++ (KCC) compiler.

tempfile="tmp$$"

"$@" 2>$tempfile

ret_code="$?"

cat $tempfile |
grep -v '^Notice: Your license to KCC_Compiler will expire in'  |
grep -v '^Contact support@kai.com, or call' |
grep -v '^renewal information\.' 1>&2

rm $tempfile

exit $ret_code
