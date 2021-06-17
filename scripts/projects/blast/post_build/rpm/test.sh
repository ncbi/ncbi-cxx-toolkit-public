#!/bin/sh -xe
# Script to run the make_rpm.py script. Assumes SRC_TARBALL exists and points
# to the output of a prepare_release build
SRC_TARBALL=/net/snowman/vol/export4/blastqa/camacho/OUTPUT/prepare_release.2016-04-16-00-11-44.build.blast-2.3.1/releaseSrc.tar.gz
INST=JUNK_DIR
rm -fr $INST
mkdir $INST
./make_rpm.py -v 2.3.1 $INST $SRC_TARBALL
rpm -qp --provides $INST/installer/ncbi-blast-2.3.1+-1.x86_64.rpm >p
rpm -qp --requires $INST/installer/ncbi-blast-2.3.1+-1.x86_64.rpm >r
# set +e
# diff p provides.new
# diff r requires.new
