#! /bin/sh

# $Id$
# Author:  Dmitriy Beloslyudtsev, NCBI  (dima@ncbi.nlm.nih.gov)
#
# Build & install DOC++ browser database for the NCBI C++ Toolkit on RAY


exec 2>&1

CVSROOT=/am/src/NCBI/vault.ncbi
NCBI=/netopt/ncbi_tools
PATH=/usr/bin:$HOME/bin:/usr/ccs/bin:$NCBI/doc++/bin
export CVSROOT NCBI PATH

SRC=/export/home/coremake/doc++
DST=/web/private/htdocs/intranet/ieb/CPP/doc++

cd `dirname $0`  ||  exit 1
BIN=`pwd`

cd $SRC || exit 1
rm -rf doc sources sources.in sources.out
time $BIN/sb_docxx.pl
cp -rp doc/* $DST
rm -rf sources

exit 0
