#! /bin/sh

# $Id$
# Author:  Dmitriy Beloslyudtsev, NCBI  (dima@ncbi.nlm.nih.gov)
#
# Build & install DOC++ browser database for the NCBI C++ Toolkit on RAY


exec 2>&1

PATH=/usr/bin:$HOME/bin:/usr/ccs/bin
CVSROOT=/am/src/NCBI/vault.ncbi
NCBI=/netopt/ncbi_tools
export CVSROOT PATH NCBI

SRC=/export/home/coremake/doc++
DST=/web/private/htdocs/intranet/ieb/CPP/doc++

cd $SRC || exit 1
rm -rf doc sources sources.in sources.out
time ./sb_docxx.pl
cp -rp doc/* /web/private/htdocs/intranet/ieb/CPP/doc++/
rm -rf sources

exit 0
