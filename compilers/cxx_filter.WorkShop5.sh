#! /bin/sh

# $Id$
# Author:  Denis Vakatov, NCBI 
#
#  Filter out redundant warnings issued by WorkShop 5.0 C++ compiler.
#  Simplify the output.

tempfile="tmp$$"

egrep -v -e '
^"/netopt/SUNWspro6/SC5\.0/include/CC/\./ios", line 148:
^"/netopt/SUNWspro6/SC5\.0/include/CC/\./ostream", line 73:
^"/netopt/SUNWspro6/SC5\.0/include/CC/\./ostream", line 347:
^"/netopt/SUNWspro6/SC5\.0/include/CC/\./ostream", line 350:
^"/netopt/SUNWspro6/SC5\.0/include/CC/\./strstream", line 147:
^"/netopt/SUNWspro6/SC5\.0/include/CC/\./strstream", line 182:
^"/netopt/SUNWspro6/SC5\.0/include/CC/\./strstream", line 222:
^"/netopt/SUNWspro6/SC5\.0/include/CC/std/errno\.h", line 20:
^"/netopt/SUNWspro6/SC5\.0/include/CC/errno_iso_SUNWCC\.h", line 16:
^"/netopt/SUNWspro6/SC5\.0/include/CC/\./fstream", line 275:
^"/netopt/SUNWspro6/SC5\.0/include/CC/\./fstream", line 319:
^"/netopt/SUNWspro6/SC5\.0/include/CC/\./fstream", line 362: 
^"/netopt/SUNWspro6/SC5\.0/include/CC/\./sstream", line 203:
^"/netopt/SUNWspro6/SC5\.0/include/CC/\./sstream", line 161:
^"/netopt/ncbi_tools/include/..*\.h", line [0-9]*: Warning \(Anachronism\): Attempt to redefine .* without using #undef\.$
^"/netopt/Sybase/clients/current/include/sybdb\.h", line [0-9]*: Warning: There are two consecutive underbars in "db__
: Warning: Could not find source for std::is[a-z][a-z]*\(int\)
: Warning: Could not find source for std::toupper\(int\)\.
: Warning: Could not find source for std::tolower\(int\)\.
: Warning: String literal converted to char\* in
:     Where: While specializing "std::basic_.*stream<char, std::char_traits<char>
:     Where: Specialized in non-template code\.
^[0-9][0-9]* Warning\(s\) detected\.
^"/netopt/SUNWspro6/SC5\.0/include/CC/\./ios", line 148: Warning:
^"/netopt/SUNWspro6/SC5\.0/include/CC/\./ostream", line 73:     Where:
^"/netopt/SUNWspro6/SC5\.0/include/CC/\./ostream", line 347:     Where: Specialized in non-template code\.
^"/netopt/SUNWspro6/SC5\.0/include/CC/\./ostream", line 350:     Where: Specialized in non-template code\.
^"/netopt/SUNWspro6/SC5\.0/include/CC/\./strstream", line 147: Warning:
^"/netopt/SUNWspro6/SC5\.0/include/CC/\./strstream", line 182: Warning:
^"/netopt/SUNWspro6/SC5\.0/include/CC/\./strstream", line 222: Warning:
^: ' |

sed '
s/std::basic_string<char, std::char_traits<char>, std::allocator<char>>/std::string/g
s/std::\([a-z_]*\)<\([^,<>]*\), std::allocator<\2>>/std::\1<\2>/g
s/std::\([a-z_]*\)<\([^,<>]*\), std::less<\2>, std::allocator<\2>>/std::\1<\2>/g
s/std::\([a-z_]*\)<\([^,<>]*\), \([^,<>]*\), std::less<\2>, std::allocator<std::pair<const \2, \3>>>/std::\1<\2, \3>/g
s/std::\([a-z_]*\)<\([^,<>]*<[^<>]*>\), std::allocator<\2>>/std::\1<\2>/g
s/std::\([a-z_]*\)<\([^,<>]*\), \([^,<>]*<[^<>]*>\), std::less<\2>, std::allocator<std::pair<const \2, \3>>>/std::\1<\2, \3>/g
' | tee $tempfile

grep '^".*", line [0-9][0-9]*: Error: ' $tempfile > /dev/null

if test $? -eq 0 ;  then
  rm $tempfile
  exit 1
else
  rm $tempfile
  exit 0
fi
