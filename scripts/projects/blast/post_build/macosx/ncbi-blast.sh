#!/bin/sh

INSTALLDIR=$1
SCRIPTDIR=$2
BLAST_VERSION=$3
PRODUCT="ncbi-blast-$BLAST_VERSION+"

if [ $# -ne 3 ] ; then
    echo "Usage: ncbi-blast.sh [installation directory] [MacOSX post-build script directory] [BLAST version]";
    exit 1;
fi

BLAST_BINS="blastn blastp blastx tblastn tblastx psiblast rpsblast rpstblastn blast_formatter deltablast legacy_blast.pl update_blastdb.pl "
MASKING_BINS="windowmasker dustmasker segmasker"
DB_BINS="blastdbcmd makeblastdb makeprofiledb makembindex blastdb_aliastool convert2blastmask blastdbcheck"
ALL_BINS="$BLAST_BINS $MASKING_BINS $DB_BINS"

rm -rf $PRODUCT.dmg $PRODUCT _stage $INSTALLDIR/installer
mkdir -p _stage/usr/local/ncbi/blast/bin _stage/usr/local/ncbi/blast/doc _stage/private/etc/paths.d
if [ $? -ne 0 ]; then
    echo FAILURE
    exit 1;
fi

cat > _stage/usr/local/ncbi/blast/doc/README.txt <<EOF
The user manual is available in http://www.ncbi.nlm.nih.gov/books/NBK1762
Release notes are available in http://www.ncbi.nlm.nih.gov/books/NBK131777
EOF
if [ $? -ne 0 ]; then
    echo FAILURE
    exit 1;
fi

cp -p $SCRIPTDIR/ncbi_blast _stage/private/etc/paths.d
if [ $? -ne 0 ]; then
    echo FAILURE
    exit 1;
fi

# This is needed because the binary ncbi-blast.pmproj has this string hard
# coded
cp -p $INSTALLDIR/LICENSE ./license.txt
for f in uninstall_ncbi_blast.zip large-Blue_ncbi_logo.tiff ncbi-blast.pmdoc welcome.txt; do
    echo copying $f to local directory
    cp -rp $SCRIPTDIR/$f .
    if [ $? -ne 0 ]; then
        echo FAILURE
        exit 1;
    fi
done

for bin in $ALL_BINS; do
    echo copying $bin
    cp -p $INSTALLDIR/bin/$bin _stage/usr/local/ncbi/blast/bin
    if [ $? -ne 0 ]; then
        echo FAILURE
        exit 1;
    fi
done

echo building package
mkdir $PRODUCT
/Developer/usr/bin/packagemaker --id gov.nih.nlm.ncbi.blast --doc ncbi-blast.pmdoc --out $PRODUCT/$PRODUCT.pkg
if [ $? -ne 0 ]; then
    echo FAILURE
    exit 1;
fi

echo copying uninstaller
cp -p uninstall_ncbi_blast.zip $PRODUCT
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
