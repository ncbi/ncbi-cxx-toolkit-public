#! /bin/sh

# $Id$
# Authors:  Denis Vakatov, NCBI; Aaron Ucko, NCBI
#
# Filter out redundant warnings issued by Sun C++ 5.9 (Studio Express) compiler.
# Simplify the output.

# Force locale settings that won't interfere with our use of non-ASCII
# single-byte characters.
LC_ALL=C
export LC_ALL

sed 's/std::basic_string<char, std::char_traits<char>, std::allocator<char>>/std::string/g
s/std::basic_\([a-z]*stream\)<char, std::char_traits<char>>/std::\1/g
:level
s/\(std::[a-z_]*\)<\([^,<>]*\), std::allocator«\2»>/\1<\2>/g
s/\(std::[a-z_]*\)<\([^,<>]*\), std::less«\2», std::allocator«\2»>/\1<\2>/g
s/\(std::[a-z_]*\)<\([^,<>]*\), \([^,<>]*\), std::less«\2», std::allocator«std::pair«const \2¸ \3»»>/\1<\2¸ \3>/g
s/\(std::[a-z_]*\)<\([^,<>]*\), \([^,<>]*«const \2¸ \([^<>]*\)»\), __rwstd::__select1st«\3¸ \2», std::less«\2», std::allocator«\3»>/\1<\2¸ [\4]>/g
:comma
s/<\([^,<>]*\), \([^<>]*\)>/<\1¸ \2>/g
t comma
s/<\([^<>]*\)>/«\1»/g
t level
y/«¸»/<,>/' | nawk -f `dirname $0`/cxx_filter.WorkShop59.awk
