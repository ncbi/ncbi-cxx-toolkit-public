#! /opt/local/bin/perl

# $Id$
# Author:  Anton Butanaev, NCBI  (butanaev@ncbi.nlm.nih.gov)
#
# Build & install source browser database for the NCBI C++ Toolkit.
#
# Using DOC++:
#    http://src.doc.ic.ac.uk/packages/Linux/sunsite.unc-mirror/apps/doctools/

retrieve();
genlists();
preprocess();
docxx();
exit;


sub retrieve
{
  $ncbi = $ENV{'NCBI'};
  open(CVS, "$ncbi/c++/scripts/cvs_core.sh sources --with-objects --without-cvs --unix|");
  while(<CVS>)
  {
    print;
  }
  print "Sources retreived...\n";
}

sub genlists
{
  open(FIND, "find . \\( -name '*.h' -o -name '*.hpp' \\) -a -print|");
  open(LISTIN, ">sources.in");
  open(LISTOUT, ">sources.out");
  while(<FIND>)
  {
    print LISTIN;
    s/(\.hp*)/$1.out/;
    print LISTOUT;
  }
  close(FIND);
  close(LISTIN);
  close(LISTOUT);
  
  open(FOOTER, ">footer");
  print FOOTER "<center>";
  print FOOTER "<a href=\"http://sunweb.ncbi.nlm.nih.gov:6224/IEB/corelib/cpp/index.html\">Manuals</a> ";
  print FOOTER "<a href=\"http://ray.nlm.nih.gov:6224/intranet/ieb/CPP/doc++/HIER.html\">Hierarchy</a> ";
  print FOOTER "<a href=\"http://ray.nlm.nih.gov:6224/intranet/ieb/CPP/doc++/index.html\">Index</a> ";
  print FOOTER "<a href=\"http://ray.nlm.nih.gov:6224/intranet/ieb/CPP/lxr/http/ident/\">Identifier search</a> ";
  print FOOTER "<a href=\"http://ray.nlm.nih.gov:6224/intranet/ieb/CPP/lxr/http/search/\">Text search</a> ";
  print FOOTER "<a href=\"http://ray.nlm.nih.gov:6224/intranet/ieb/CPP/lxr/http/find/\">File search</a> ";
  print FOOTER "<a href=\"http://ray.nlm.nih.gov:6224/intranet/ieb/CPP/lxr/http/source/\">Source code</a> ";
  print FOOTER "</center></body></html>";
  close(FOOTER);
  
  print "File list built...\n";
}

sub docxx
{
  open(DOCXX, "doc++ -v -H -S -B footer -d doc -I sources.out|");
  while(<DOCXX>)
  {
    print;
  }
  print "Documentation generated...\n";
}

sub preprocess
{
  $href1 = '<font size=-1><b><a href="http://ray.nlm.nih.gov:6224/intranet/ieb/CPP/lxr/http/ident?i=';
  $href2 = '">Locate <i>';
  $href3 = '</i></a></b> in the source code</font>';
  open(LISTIN, "sources.in");
  while(<LISTIN>)
  {
    chomp;
    open(IN, "$_");
    open(OUT, ">$_.out");
    while(<IN>)
    {
      s/\/\/+/\/\//;
      s/\/\*+/\/\*/;
      s/BEGIN_NCBI_SCOPE//;
      s/END_NCBI_SCOPE//;
      s/BEGIN_objects_SCOPE//;
      s/END_objects_SCOPE//;
      s/^(namespace [a-zA-Z]+)//;
      s/^typedef enum/enum/;

      next if(/def.*_HP*$/);
      
      $prefix = '';
      if(/^template/)
      {
        $prefix = $_;
        $_ = <IN>;
      }
      
      if(/^ *class/ && ! /;\n$/)
      {
        s/^( *class +([_A-Za-z][_A-Za-z0-9]*))/\n\/\/\/$href1$2$href2$2$href3\n$prefix$1/;
      }
      elsif(/^ *struct/ && ! /;\n$/)
      {
        s/^( *struct +([_A-Za-z][_A-Za-z0-9]*))/\n\/\/\/$href1$2$href2$2$href3\n$prefix$1/;
      }
      elsif(/^ *enum/ && ! /;\n$/)
      {
        s/^( *enum +([_A-Za-z][_A-Za-z0-9]*))/\n\/\/\/$href1$2$href2$2$href3\n$prefix$1/;
      }
      elsif(/^ *typedef/)
      {
        s/^( *typedef.* +([_A-Za-z][_A-Za-z0-9]*));$/\n\/\/\/$href1$2$href2$2$href3\n$prefix$1/;
      }
      elsif(/^ *extern/)
      {
        s/^( *extern.* +([_A-Za-z][_A-Za-z0-9]*) *\(.*)$/\n\/\/\/$href1$2$href2$2$href3\n$prefix$1/;
      }
      elsif(/^ *# *define/)
      {
        s/^( *# *define +([_A-Za-z][_A-Za-z0-9]*) *\(*.*)$/\n\/\/\/$href1$2$href2$2$href3\n$prefix$1/;
      }
      {
        if(! /^ *if[^A-Za-z_]/ && ! /^ *return[^A-Za-z_]/ && 
           ! /^ *while[^A-Za-z_]/ && ! / *for[^A-Za-z_]/ && 
           ! / *throw[^A-Za-z_]/ && ! /::/)
        {
          s/^(    [a-zA-Z])/\n    \/\/\/\n$1/;
          s/^(        [a-zA-Z])/\n        \/\/\/\n$1/;
        }
        $_ = $prefix . $_;
      }

      print OUT;
    }
    close IN;
    close OUT;
  }

  print "Files preprocessed...\n";
}
