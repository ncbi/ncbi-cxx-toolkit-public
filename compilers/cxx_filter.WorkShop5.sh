#! /bin/sh

# $Id$
# Author:  Denis Vakatov, NCBI 
#
#  Filter out redundant warnings issued by WorkShop 5.0 C++ compiler.
#  Simplify the output.

egrep -v -e '
^"/netopt/SUNWspro6/SC5\.0/include/CC/\./ios", line 148:
^"/netopt/SUNWspro6/SC5\.0/include/CC/\./ostream", line 73:
^"/netopt/SUNWspro6/SC5\.0/include/CC/\./ostream", line 347:
^"/netopt/SUNWspro6/SC5\.0/include/CC/\./ostream", line 350:
^"/netopt/SUNWspro6/SC5\.0/include/CC/\./strstream", line 147:
^"/netopt/SUNWspro6/SC5\.0/include/CC/\./strstream", line 182:
^"/netopt/SUNWspro6/SC5\.0/include/CC/\./strstream", line 222:
^"/netopt/SUNWspro6/SC5\.0/include/CC/std/errno.h", line 20:
^"/netopt/SUNWspro6/SC5.0/include/CC/errno_iso_SUNWCC.h", line 16:
^"/netopt/SUNWspro6/SC5.0/include/CC/\./fstream", line 275:
^"/netopt/SUNWspro6/SC5.0/include/CC/\./fstream", line 319:
^"/netopt/ncbi_tools/include/..*\.h", line .*: Warning \(Anachronism\): Attempt to redefine .* without using #undef\.$
: Warning: Could not find source for std::is[a-z][a-z]*(int)
: Warning: Could not find source for std::toupper\(int\)\.
: Warning: Could not find source for std::tolower\(int\)\.
: Warning: String literal converted to char\* in
:     Where: While specializing "std::basic_.*fstream<char, std::char_traits<char>>".
:     Where: Specialized in non-template code.
^[0-9][0-9]* Warning\(s\) detected.
^: 
^touch .*\.dep$' |

sed '
s/std::basic_string<char, std::char_traits<char>, std::allocator<char>>/std::string/g
s/std::\([a-z]*\)<\([^,<>]*\), std::allocator<\2>>/std::\1<\2>/g
s/std::\([a-z]*\)<\([^,<>]*\), std::less<\2>, std::allocator<\2>>/std::\1<\2>/g
s/std::\([a-z]*\)<\([^,<>]*\), \([^,<>]*\), std::less<\2>, std::allocator<std::pair<const \2, \3>>>/std::\1<\2, \3>/g
s/std::\([a-z]*\)<\([^,<>]*<[^<>]*>\), std::allocator<\2>>/std::\1<\2>/g
s/std::\([a-z]*\)<\([^,<>]*\), \([^,<>]*<[^<>]*>\), std::less<\2>, std::allocator<std::pair<const \2, \3>>>/std::\1<\2, \3>/g
'
