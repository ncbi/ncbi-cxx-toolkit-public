#!/usr/bin/nawk -f

# $Id$
# Author:  Aaron Ucko, NCBI 
#
#  Filter out redundant warnings issued by WorkShop 6u2 C++ 5.3 compiler.

# Treat multiline messages (with "Where:") as single units; cope with
# HTML output when run under the GUI.  (Openers stick to following
# rather than preceding message lines.)
{
  if (NR == 1) {
    message = $0;
  } else if (/^<[A-Za-z]+>$/) {
    openers = openers $0 "\n";
  } else if (/    Where: / || /SUNW-internal-info-sign/ || /^<\/[A-Za-z]+>$/) {
    message = message "\n" openers $0;
    openers = "";
  } else {
    print_if_interesting();
    message = openers $0;
    openers = "";
  }
}
END { if (length(message)) print_if_interesting(); }


# Set exit status based on the presence of error messages.
# (Only the last pipeline stage's status counts.)
BEGIN { status=0 }
/^".+", (line [0-9]+(<\/A>)?: )?Error:/      { status=1 }
/^Fatal Error/                               { status=1 }
/^ >> Assertion:/                            { status=1 }
/^Error: /                                   { status=1 }
/:error:Error:/                              { status=1 }
/^compiler\([^)]+\) error:/                  { status=1 }
/^Undefined/                                 { status=1 }
/^Could not open file /                      { status=1 }
/: fatal:/                                   { status=1 }
END { exit status } # This must be the last END block.


function print_if_interesting()
{
  # Warning counts are almost certainly going to be too high; drop them.
  if (message ~ /^[0-9]+ Warning\(s\) detected\.$/)
    return;

  # We only want to suppress warnings; let everything else through.
  if (message !~ /Warning/) {
    print message;
    return;
  }

  m = message;
  if (message ~ /<HTML>/) {
      gsub(/\n/,        " ",  m);
      gsub(/<[^>]*> */, "",   m); # Strip tags.
      gsub(/&lt;/,      "<",  m); # Expand common entities.
      gsub(/&gt;/,      ">",  m);
      gsub(/&quot;/,    "\"", m);
      gsub(/&amp;/,     "&",  m);
  }

  if (0 ||
      m ~ /Warning: ".+" is too large and will not be expanded inline\./ ||
      m ~ /Warning: ".+" is too large to generate inline, consider writing it yourself./ ||
      m ~ /Warning: Could not find source for ncbi::CTreeIteratorTmpl<ncbi::C(Const)?TreeLevelIterator>::/ ||
      m ~ /Warning: Could not find source for ncbi::CTypes?IteratorBase<ncbi::CTreeIterator(Tmpl<ncbi::C(Const)?TreeLevelIterator>)?>::/ ||
      m ~ /Where: While instantiating "(__rw)?std::.*(<.*>)?::__((de)?allocate_.*|unLink)\(\)"/ ||
      m ~ /^".*\/include\/CC\/Cstd\/\.\/.*", line [0-9]*: .*should not initialize a non-const reference with a temporary\./ ||
      m ~ /^".*\/include\/CC\/Cstd\/\.\/fstream", line (277|321|364): .*::rdbuf hides/ ||
      m ~ /^".*\/include\/CC\/Cstd\/\.\/ostream", line 331: Warning: The else-branch should return a value/ ||
      m ~ /^".*\/include\/CC\/Cstd\/\.\/sstream", line (130|165|207): .*::rdbuf hides/ ||
      m ~ /^".*\/include\/CC\/std\/errno\.h", line 20: .*extra text on this line/ ||
      m ~ /^".*\/include\/FL\/Fl_.+\.H", line [0-9]+: Warning: Fl_.+ hides the function Fl_.+/ ||
      m ~ /^".*\/include\/fox\/FX.+\.h", line [0-9]+: Warning: Comparing different enum types "enum" and "enum"\./ ||
      m ~ /^".*\/include\/fox\/FX.+\.h", line [0-9]+: Warning: FX.+ hides the function FX/ ||
      m ~ /^".*\/include\/fox\/FXObject\.h".+two consecutive underbars in "__FXMETACLASSINITIALIZER__"\./ ||
      m ~ /^".*\/include\/html\/jsmenu\.hpp", line [0-9]+: Warning: ncbi::CHTMLPopupMenu::SetAttribute hides the function ncbi::CNCBINode::SetAttribute/ ||
      m ~ /^".*\/include\/internal\/idx\/idcont.hpp", line [0-9]+: Warning: ncbi::CPmDbIdContainerUid::(Unc|C)ompress hides the virtual function ncbi::CPmDbIdContainer::/ ||
      m ~ /^".*\/include\/internal\/webenv2\/[a-z]+\.hpp", line [0-9]+: Warning: ncbi::CQ[A-Za-z]+::FromAsn hides the function/ ||
      m ~ /^".*\/include\/sybdb\.h".*two consecutive underbars in "db__.*"\./ ||
      m ~ /^".*\/include\/serial\/objostr[a-z]+\.hpp".*hides the function ncbi::CObjectOStream::WriteClassMember/ ||
      m ~ /two consecutive underbars in "___errno"\./ ||
      0)
    return;

  # Default: let it through
  print message;
  # if (message ~ /<HTML>/) print "[" m "]";
}
