#! /opt/local/bin/perl

# $Id$
# Authors:  Anton Butanaev, Denis Vakatov (NCBI)
#
# Build & install source browser database for the NCBI C++ Toolkit.
#
# Using DOC++:
#    http://src.doc.ic.ac.uk/packages/Linux/sunsite.unc-mirror/apps/doctools/


# localization
$NAME_DOCXX  = "docxx";
$BASE_DOCXX  = ".";
$BASE_ROOT   = "..";
$BASE_LXR    = "../lxr";
$BASE_SSI    = "../ssi";
$HREF_MANUAL = "../index.html";
$ENV{'PATH'} = "/usr/bin:/netopt/ncbi_tools/doc++/bin";

# redirect STDERR to STDOUT, and set unbuffered i/o for STDOUT
close STDERR;
open STDERR, ">&STDOUT";
select((select(STDOUT), $|=1)[$[]);

# go to the working dir
$tmp = `dirname $0`;
chomp $tmp;
chdir $tmp  or  die "$!";
mkdir $NAME_DOCXX, 0755;
chdir $NAME_DOCXX or  die "$!";

# compose HTML page header and footer
Compose_HeaderAndFooter();

# find and preprocess all header files to prepare for the run through DOC++
PrepareFiles();

# run DOC++
open(DOCXX, "doc++ -f -v -H -S -B footer -T header -I sources.lst|");
while(<DOCXX>)
{
  print;
}
# fix HTML headers in main files
FixHtmlHeader("index.html","Table of Contents");
FixHtmlHeader("HIER.html","Hierarchy of Classes");

print "Documentation generated.\n";

# cleanup
system qq{ rm -r src include sources.lst footer header };

# done
exit;


#################################

sub Compose_HeaderAndFooter
{
  open(HEADER, ">header");
  print HEADER "<!--#set var=\"DOCROOT\" value=\"$BASE_ROOT\" -->\n";
  print HEADER "<!--#include virtual=\"$BASE_SSI/header.shtml\" -->\n";
  print HEADER "<center>";
  print HEADER "<a href=\"$HREF_MANUAL\">[Manuals]</a>&nbsp;&nbsp;";
  print HEADER "<a href=\"$BASE_DOCXX/HIER.html\">[Hierarchy]</a>&nbsp;&nbsp;";
  print HEADER "<a href=\"$BASE_DOCXX/index.html\">[Index]</a>";
  print HEADER "</center><br>";
  close(HEADER);

  open(FOOTER, ">footer");
  print FOOTER "<center>";
  print FOOTER "<a href=\"$HREF_MANUAL\">[Manuals]</a>&nbsp;&nbsp;";
  print FOOTER "<a href=\"$BASE_DOCXX/HIER.html\">[Hierarchy]</a>&nbsp;&nbsp;";
  print FOOTER "<a href=\"$BASE_DOCXX/index.html\">[Index]</a>";
  print FOOTER "</center>";
  print FOOTER "<!--#include virtual=\"$BASE_SSI/footer.shtml\" -->\n";
  close(FOOTER);
}


#################################

sub PrepareFiles
{
  $href1 = "<font size=-1><i>Locate <b><a href=\"$BASE_LXR/ident?i=";
  $href2 = "&d=smart\">";
  $href3 = "</a></b> in the source code</i></font>";

  open(FINDDIR, "find ../.. -type d ! -name CVS | sed 's%^\.\./\.\./%%'|egrep 'include|src'|");
  while(<FINDDIR>) {
    chomp;
    mkdir "$_", 0755 or die "$!";
  }
  close FINDDIR;

  open(FIND, "find ../.. ! -type d \\( -name '*.h' -o -name '*.hpp' -o -name '*.inl' \\) -a -print | egrep 'include|src'|");
  open(LIST, ">sources.lst");

  while(<FIND>) {
    $src = "$_";
    s/^\.\.\/\.\.\///;
    $targ = "$_";

    print LIST "$targ";
    chomp $src;
    chomp $targ;

    open(IN, "$src");
    open(OUT, ">$targ");

    while(<IN>) {
      s/BEGIN_NCBI_SCOPE//;
      s/END_NCBI_SCOPE//;
      s/BEGIN_objects_SCOPE//;
      s/END_objects_SCOPE//;
      s/^(namespace  *[a-zA-Z]+)//;
      s/typedef  *enum/enum/;
      s/typedef  *struct/struct/;

      next if(/def.*_HP*$/);
      
      $prefix = '';
      if(/^template/) {
        $prefix = $_;
        $_ = <IN>;
      }
      
      if(/^ *class/ && ! /;\n$/) {
        s/^( *class +([_A-Za-z][_A-Za-z0-9]*))/\n\/\/\/$href1$2$href2$2$href3\n$prefix$1/;
      } elsif(/^ *struct/ && ! /;\n$/) {
        s/^( *struct +([_A-Za-z][_A-Za-z0-9]*))/\n\/\/\/$href1$2$href2$2$href3\n$prefix$1/;
      } elsif(/^ *enum/ && ! /;\n$/) {
        s/^( *enum +([_A-Za-z][_A-Za-z0-9]*))/\n\/\/\/$href1$2$href2$2$href3\n$prefix$1/;
      } elsif(/^ *typedef/) {
        s/^( *typedef.* +([_A-Za-z][_A-Za-z0-9]*));$/\n\/\/\/$href1$2$href2$2$href3\n$prefix$1/;
      } elsif(/^ *extern/) {
        s/^( *extern.* +([_A-Za-z][_A-Za-z0-9]*) *\(.*)$/\n\/\/\/$href1$2$href2$2$href3\n$prefix$1/;
      } elsif(/^ *\# *define/) {
        s/^( *\# *define +([_A-Za-z][_A-Za-z0-9]*) *\(*.*)$/\n\/\/\/$href1$2$href2$2$href3\n$prefix$1/;
      }

      if(! /^ *if[^A-Za-z_]/ && ! /^ *return[^A-Za-z_]/ && 
         ! /^ *while[^A-Za-z_]/ && ! / *for[^A-Za-z_]/ && 
         ! / *throw[^A-Za-z_]/ && ! /::/) {
        s/^(    [a-zA-Z])/\n    \/\/\/\n$1/;
        s/^(        [a-zA-Z])/\n        \/\/\/\n$1/;
      }

      $_ = $prefix . $_;

      print OUT;
    }

    close OUT;
    close IN;
  }

  close(FIND);
  close(LIST);
}


#################################

sub FixHtmlHeader
{
  local $fname = shift;
  local $title = shift;

  open(IN, "$fname");
  open(OUT, ">$fname.tmp");
  
  print OUT "<!--#set var=\"TITLE\" value=\"$title\" -->\n";
  open(HEADER, "header");
  while(<HEADER>) {
    print OUT "$_";
  }
  local $i = 0;
  while(<IN>) {
	if ($i>5) {
       print OUT "$_";
	 }
    $i++;
  }

  close(IN);
  close(OUT);
  rename("$fname.tmp", $fname);
}
