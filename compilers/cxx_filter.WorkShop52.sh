#! /bin/sh

# $Id$
# Author:  Denis Vakatov, NCBI 
#
#  Filter out redundant warnings issued by WorkShop 6 C++ 5.1 compiler.
#  Simplify the output.

tempfile="tmp$$"

std_incpath='^"/netopt/forte6u1/WS6U1/include/CC/'
std_incpath_Cstd=${std_incpath}'Cstd/'

egrep -v -e '
'$std_incpath_Cstd'.*Conversion of 64 bit type value to \".*\" causes truncation
'$std_incpath'std/errno\.h", line 20:
'$std_incpath'errno_iso_SUNWCC\.h", line 16:
'$std_incpath_Cstd'\./fstream", line 275:
'$std_incpath_Cstd'\./fstream", line 319:
'$std_incpath_Cstd'\./fstream", line 362: 
'$std_incpath_Cstd'\./sstream", line 203:
'$std_incpath_Cstd'\./sstream", line 161:
'$std_incpath_Cstd'\./sstream", line 126:
^"/netopt/ncbi_tools/include/..*\.h", line [0-9]*: Warning \(Anachronism\): Attempt to redefine .* without using #undef\.$
^"/netopt/Sybase/clients/current/include/sybdb\.h", line [0-9]*: Warning: There are two consecutive underbars in "db__
Warning: Could not find source for
: Warning: String literal converted to char\* in
:     Where: While specializing "std::basic_.*stream<char, std::char_traits<char>
:     Where: Specialized in non-template code\.
^[0-9][0-9]* Warning\(s\) detected\.
'$std_incpath_Cstd'\./ios", line 148: Warning:
'$std_incpath_Cstd'\./ostream", line 73:     Where:
'$std_incpath_Cstd'\./ostream", line 347:     Where: Specialized in non-template code\.
'$std_incpath_Cstd'\./ostream", line 350:     Where: Specialized in non-template code\.
'$std_incpath_Cstd'\./strstream", line 147: Warning:
'$std_incpath_Cstd'\./strstream", line 182: Warning:
'$std_incpath_Cstd'\./strstream", line 222: Warning:
fox/FX.*\.h", line [0-9][0-9]*: Warning:
^: ' |

sed '
s/std::basic_string<char, std::char_traits<char>, std::allocator<char>>/std::string/g
s/std::\([a-z_]*\)<\([^,<>]*\), std::allocator<\2>>/std::\1<\2>/g
s/std::\([a-z_]*\)<\([^,<>]*\), std::less<\2>, std::allocator<\2>>/std::\1<\2>/g
s/std::\([a-z_]*\)<\([^,<>]*\), \([^,<>]*\), std::less<\2>, std::allocator<std::pair<const \2, \3>>>/std::\1<\2, \3>/g
s/std::\([a-z_]*\)<\([^,<>]*<[^<>]*>\), std::allocator<\2>>/std::\1<\2>/g
s/std::\([a-z_]*\)<\([^,<>]*\), \([^,<>]*<[^<>]*>\), std::less<\2>, std::allocator<std::pair<const \2, \3>>>/std::\1<\2, \3>/g
' | tee $tempfile

egrep -e '
^".*", line [0-9][0-9]*: Error: 
^Fatal Error 
^ \>\> Assertion:
^Error: ' $tempfile > /dev/null

if test $? -eq 0 ;  then
  rm $tempfile
  exit 1
else
  rm $tempfile
  exit 0
fi
