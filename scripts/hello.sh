#! /bin/sh
# $Id$
# Author:  Denis Vakatov (vakatov@ncbi.nlm.nih.gov)

# To retrieve, deploy, and build CGI demo project HELLO


#################################
# Useful paths, etc.

project="hello"
cvs_topdir="internal/c++"

scriptdir=`dirname $0`
script=`basename $0`
tmpdir="tmp.$script.$$"
target=`basename $project`
targetdir=`pwd`/$target


#################################
# Usage

Usage()
{
  cat <<EOF
USAGE:  $script
SYNOPSIS:
   Checkout demo project HELLO.CGI from the NCBI CVS tree,
   then build it using pre-built NCBI C++ Toolkit.

ERROR:  $1
EOF
  exit 1
}


#################################
# Deploy project sources

mkdir $targetdir  ||  Usage "mkdir $targetdir"
mkdir $tmpdir  ||  Usage "mkdir $tmpdir"
cd    $tmpdir  ||  Usage "cd $tmpdir"

$scriptdir/import_project.sh $project
if test $? -ne 0 ; then
  Usage "Cannot retrieve from CVS"
fi

find . -name CVS -type d -exec rm -r {} \;
cp -pr $cvs_topdir/include/$project/* $targetdir
cp -pr $cvs_topdir/src/$project/* $targetdir
cd ..
rm -r $tmpdir


#################################
# Adjust project makefiles

cd $targetdir  ||  Usage "cd $targetdir"
for mfile in Makefile.*_* ; do
    sed 's/^LOCAL_CPPFLAGS *=.*/LOCAL_CPPFLAGS = -I.. -I./' < $mfile > makefile.$$
    rm $mfile
    mv makefile.$$ $mfile
done


#################################
# Build & Run

make -f Makefile.hello_app
mv hello hello.cgi
./hello.cgi > hello.html

make -f Makefile.fasthello_app
mv fasthello hello.fcgi

echo DONE.
echo See the result in:  `pwd`/hello.html
