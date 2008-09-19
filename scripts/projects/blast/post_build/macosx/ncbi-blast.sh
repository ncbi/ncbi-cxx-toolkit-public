#!/bin/sh

INSTALLDIR=$1
SCRIPTDIR=$2
BLAST_VERSION=$3
PRODUCT="ncbi-blast-$BLAST_VERSION+"

if [ $# -ne 3 ] ; then
    echo "Usage: ncbi-blast.sh [installation directory] [MacOSX post-build script directory] [BLAST version]";
    exit 1;
fi

BLAST_BINS="blastn blastp blastx tblastn tblastx psiblast rpsblast rpstblastn legacy_blast.pl"
MASKING_BINS="windowmasker dustmasker segmasker"
DB_BINS="blastdbcmd makeblastdb makembindex"
ALL_BINS="$BLAST_BINS $MASKING_BINS $DB_BINS"

rm -rf $PRODUCT.dmg $PRODUCT _stage $INSTALLDIR/installer
mkdir -p _stage/usr/local/ncbi/blast/bin _stage/usr/local/ncbi/blast/doc _stage/private/etc/paths.d

cp -p $SCRIPTDIR/ncbi_blast _stage/private/etc/paths.d

# This is needed because the binary ncbi-blast.pmproj has this string hard
# coded
cp -p $INSTALLDIR/LICENSE ./license.txt
for f in large-Blue_ncbi_logo.tiff ncbi-blast.pmdoc welcome.txt; do
    echo copying $f to local directory
    cp -rp $SCRIPTDIR/$f .
done

for bin in $ALL_BINS; do
    echo copying $bin
    cp -p $INSTALLDIR/bin/$bin _stage/usr/local/ncbi/blast/bin
done

echo building package
mkdir $PRODUCT
/Developer/Tools/packagemaker --doc ncbi-blast.pmdoc --out $PRODUCT/$PRODUCT.pkg

echo creating disk image
/usr/bin/hdiutil create $PRODUCT.dmg -srcfolder $PRODUCT

echo moving disk image
mkdir $INSTALLDIR/installer
mv $PRODUCT.dmg $INSTALLDIR/installer

echo done
rm -rf _stage $PRODUCT
