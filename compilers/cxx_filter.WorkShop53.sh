#! /bin/sh

# $Id$
# Authors:  Denis Vakatov, NCBI; Aaron Ucko, NCBI
#
#  Filter out redundant warnings issued by WorkShop 6u2 C++ 5.3 compiler.
#  Simplify the output.

sed '
s/std::basic_string<char, std::char_traits<char>, std::allocator<char>>/std::string/g
s/std::basic_\([a-z]*stream\)<char, std::char_traits<char>>/std::\1/g
s/std::\([a-z_]*\)<\([^,<>]*\), std::allocator<\2>>/std::\1<\2>/g
s/std::\([a-z_]*\)<\([^,<>]*\), std::less<\2>, std::allocator<\2>>/std::\1<\2>/g
s/std::\([a-z_]*\)<\([^,<>]*\), \([^,<>]*\), std::less<\2>, std::allocator<std::pair<const \2, \3>>>/std::\1<\2, \3>/g
s/std::\([a-z_]*\)<\([^,<>]*<[^<>]*>\), std::allocator<\2>>/std::\1<\2>/g
s/std::\([a-z_]*\)<\([^,<>]*\), \([^,<>]*<[^<>]*>\), std::less<\2>, std::allocator<std::pair<const \2, \3>>>/std::\1<\2, \3>/g
' | nawk -f `dirname $0`/cxx_filter.WorkShop53.awk
