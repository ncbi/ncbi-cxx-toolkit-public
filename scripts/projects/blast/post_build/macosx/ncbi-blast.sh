#!/bin/sh

SRC=$1
PLATFORM=$2

if [ $# -ne 2 ] ; then
    echo "Usage: ncbi-blast.sh [source_directory_for_binaries] PowerMAC";
    exit 1;
fi

if [ "PowerMAC" != "$PLATFORM" ] ; then
    exit 0;
fi

BLAST_BINS="blastn blastp blastx tblastn tblastx psiblast rpsblast rpstblastn legacy_blast.pl"
MASKING_BINS="windowmasker dustmasker segmasker"
DB_BINS="blastdbcmd makeblastdb makembindex"
ALL_BINS=$(BLAST_BINS) $(MASKING_BINS) $(DB_BINS)

rm -rf BLAST.dmg

rm -rf _stage
mkdir _stage
mkdir _stage/bin
mkdir _stage/doc

for bin in $ALL_BINS
do
echo copying $bin
cp -p $SRC/$bin _stage/bin
done

echo building package
rm -rf BLAST
mkdir BLAST
/Developer/Tools/packagemaker -build -proj ncbi-blast.pmproj -p BLAST/BLAST.pkg

echo creating disk image
hdiutil create BLAST.dmg -srcfolder BLAST

echo done
rm -rf _stage
rm -rf BLAST
