#!/usr/bin/nawk -f

# $Id$
# Author:  Aaron Ucko, NCBI 
#
#  Filter out redundant warnings issued by WorkShop 6u2 C++ 5.3 compiler.

# Treat multiline messages (with "Where:") as single units.
{
  if (NR == 1) {
    m = $0;
  } else if (/    Where: /) {
    m = m "\n" $0;
  } else {
    print_if_interesting();
    m = $0;
  }
}
END { print_if_interesting() }


# Set exit status based on the presence of error messages.
# (Only the last pipeline stage's status counts.)
BEGIN { status=0 }
/^".*", line [0-9]+: Error:/ { status=1 }
/^Fatal Error/               { status=1 }
/^ >> Assertion:/            { status=1 }
/^Error: /                   { status=1 }
END { exit status } # This must be the last END block.


function print_if_interesting()
{
  # Warning counts are almost certainly going to be too high; drop them.
  if (m ~ /^[0-9]+ Warning\(s\) detected\.$/)
    return;

  # We only want to suppress warnings; let everything else through.
  if (m !~ /Warning/) {
    print m;
    return;
  }

  if (0 ||
      m ~ /Warning: Could not find source for ncbi::CTreeIteratorTmpl<ncbi::C(Const)?TreeLevelIterator>::/ ||
      m ~ /Warning: Could not find source for ncbi::CTypes?IteratorBase<ncbi::CTreeIterator(Tmpl<ncbi::C(Const)?TreeLevelIterator>)?>::/ ||
      m ~ /Where: While instantiating "(__rw)?std::.*<.*>::__(de)?allocate_.*()"/ ||
      m ~ /^".*\/include\/CC\/Cstd\/\.\/algorithm.cc", line 427: .*non-const reference/ ||
      m ~ /^".*\/include\/CC\/Cstd\/\.\/deque", line (639|671|679): .*non-const reference/ ||
      m ~ /^".*\/include\/CC\/Cstd\/\.\/fstream", line (277|321|364): .*::rdbuf hides/ ||
      m ~ /^".*\/include\/CC\/Cstd\/\.\/list", line (543|544): .*non-const reference/ ||
      m ~ /^".*\/include\/CC\/Cstd\/\.\/sstream", line 165: .*::rdbuf hides/ ||
      m ~ /^".*\/include\/CC\/Cstd\/\.\/string", line (1622|1640): .*non-const reference/ ||
      m ~ /^".*\/include\/CC\/Cstd\/\.\/vector", line (147|156|161|268|273|310|318|399|463|472|483|513|1146): .*non-const reference/ ||
      m ~ /^".*\/include\/CC\/Cstd\/\.\/vector.cc", line 113: .*non-const reference/ ||
      m ~ /^".*\/include\/CC\/std\/errno\.h", line 20: .*extra text on this line/ ||
      m ~ /^".*\/include\/sybdb\.h".*two consecutive underbars in "db__.*"\./ ||
      m ~ /^".*\/include\/serial\/objostr.*\.hpp".*hides the function ncbi::CObjectOStream::WriteClassMember/ ||
      m ~ /two consecutive underbars in "___errno"\./ ||
      0)
    return;

  # Default: let it through
  print m;
}
