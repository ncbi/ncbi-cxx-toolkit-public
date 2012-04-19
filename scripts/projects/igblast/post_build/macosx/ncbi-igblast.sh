#!/bin/sh

INSTALLDIR=$1
SCRIPTDIR=$2
BLAST_VERSION=$3
PRODUCT="ncbi-igblast-$BLAST_VERSION"

if [ $# -ne 3 ] ; then
    echo "Usage: ncbi-igblast.sh [installation directory] [MacOSX post-build script directory] [BLAST version]";
    exit 1;
fi

BLAST_BINS="igblastn igblastp"
DATA_DIRS="optional_file internal_data"
ALL_BINS="$BLAST_BINS"

rm -rf $PRODUCT.dmg $PRODUCT _stage $INSTALLDIR/installer
mkdir -p _stage/usr/local/ncbi/igblast/bin _stage/usr/local/ncbi/igblast/doc \
         _stage/usr/local/ncbi/igblast/data _stage/private/etc/paths.d
if [ $? -ne 0 ]; then
    echo FAILURE
    exit 1;
fi

cp $INSTALLDIR/README _stage/usr/local/ncbi/igblast/doc/README.txt
if [ $? -ne 0 ]; then
    echo "FAILED to copy $INSTALLDIR/README"
    exit 1;
fi

cp -p $SCRIPTDIR/ncbi_igblast _stage/private/etc/paths.d
if [ $? -ne 0 ]; then
    echo FAILURE
    exit 1;
fi

# This is needed because the binary ncbi-blast.pmproj has this string hard
# coded
cp -p $INSTALLDIR/LICENSE ./license.txt
for f in uninstall_ncbi_igblast.zip large-Blue_ncbi_logo.tiff ncbi-igblast.pmdoc welcome.txt; do
    echo copying $f to local directory
    cp -rp $SCRIPTDIR/$f .
    if [ $? -ne 0 ]; then
        echo FAILURE
        exit 1;
    fi
done

for bin in $ALL_BINS; do
    echo copying $bin
    cp -p $INSTALLDIR/bin/$bin _stage/usr/local/ncbi/igblast/bin
    if [ $? -ne 0 ]; then
        echo FAILURE
        exit 1;
    fi
done

for dir in $DATA_DIRS; do
    echo copying $SCRIPTDIR/../../../../../src/app/igblast/$dir
    cp -R $SCRIPTDIR/../../../../../src/app/igblast/$dir _stage/usr/local/ncbi/igblast/data
    if [ $? -ne 0 ]; then
        echo FAILURE
        exit 1;
    fi
done

echo building package
mkdir $PRODUCT
/Developer/usr/bin/packagemaker --id gov.nih.nlm.ncbi.blast --doc ncbi-igblast.pmdoc --out $PRODUCT/$PRODUCT.pkg
if [ $? -ne 0 ]; then
    echo FAILURE
    exit 1;
fi

echo copying uninstaller
cp -p uninstall_ncbi_igblast.zip $PRODUCT
if [ $? -ne 0 ]; then
    echo FAILURE
    exit 1;
fi

echo creating disk image
/usr/bin/hdiutil create $PRODUCT.dmg -srcfolder $PRODUCT
if [ $? -ne 0 ]; then
    echo FAILURE
    exit 1;
fi

echo moving disk image
mkdir $INSTALLDIR/installer
if [ $? -ne 0 ]; then
    echo FAILURE
    exit 1;
fi
mv $PRODUCT.dmg $INSTALLDIR/installer
if [ $? -ne 0 ]; then
    echo FAILURE
    exit 1;
fi

echo done
rm -rf _stage $PRODUCT
