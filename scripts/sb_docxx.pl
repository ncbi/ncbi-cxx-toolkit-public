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
  print "File list built...\n";
}

sub docxx
{
  open(DOCXX, "doc++ -v -H -S -d doc -I sources.out|");
  while(<DOCXX>)
  {
    print;
  }
  print "Documentation generated...\n";
}

sub preprocess
{
  $href1 = '<b><a href="http://ray.nlm.nih.gov:6224/intranet/ieb/CPP/lxr/http/ident?i=';
  $href2 = '">Locate <i>';
  $href3 = '</i></a></b> in the source code';
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
      s/^(namespace [a-zA-Z]+)//;
      s/^extern/\n\/\/\/\nextern/;
      s/^typedef/\n\/\/\/\ntypedef/;

      next if(/def.*_HP*$/);

      if(/^template/)
      {
        s/^template/\n\/\/\/\ntemplate/;
        print OUT;
        $_ = <IN>;
        print OUT;
        next;
      }
      
      if(/ *class/)
      {
        s/^( *class +([_A-Za-z][<>:_A-Za-z0-9]*) .*[^;]\n)$/\n\/\/\/$href1$2$href2$2$href3\n$1/;
      }
      elsif(/ *struct/)
      {
        s/^( *struct +([_A-Za-z][<>:_A-Za-z0-9]*) .*[^;]\n)$/\n\/\/\/$href1$2$href2$2$href3\n$1/;
      }
      else
      {
        s/^(    [a-zA-Z])/\n    \/\/\/\n$1/;
        s/^(        [a-zA-Z])/\n        \/\/\/\n$1/;
      }

      print OUT;
    }
    close IN;
    close OUT;
  }

  print "Files preprocessed...\n";
}
