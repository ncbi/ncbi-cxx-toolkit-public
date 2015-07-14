#!/bin/sh
# $Id$
set -e
exec 3>&1 >&2
root=`dirname $0`/../../local
mkdir -p $root
cd $root
root=`pwd`
ptb_ini=../src/build-system/project_tree_builder.ini
tag=`sed -ne 's,.*/vdb-versions/,,p' $ptb_ini`
name=ncbi-vdb${tag+"-$tag"}

mkdir -p src
cd src
if [ -d ncbi-vdb/.git ]; then
    (cd ncbi-vdb  &&  git fetch)
else
    git clone https://github.com/ncbi/ncbi-vdb.git
fi
if [ -d ngs/.git ]; then
    (cd ngs  &&  git fetch)
else
    git clone https://github.com/ncbi/ngs.git
fi
cd ncbi-vdb
git checkout "${tag:-master}"
./configure --prefix=$root/$name --build-prefix=$root/build/$name
make
make install
if [ -f $root/$name/include/klib/rc.h ]; then
    :
else
    (cd interfaces  &&  tar cf - *)  |  (cd $root/$name/include  &&  tar xf -)
fi
cat <<EOF

----------------------------------------------------------------------
NCBI VDB libraries successfully built and installed to
$root/$name

To take advantage of them, please configure the C++ Toolkit
--with-vdb=$root/$name
----------------------------------------------------------------------
EOF

echo $root/$name >&3
