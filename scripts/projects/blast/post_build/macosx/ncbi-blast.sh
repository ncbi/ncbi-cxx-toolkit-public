#!/bin/sh -xe

INSTALLDIR=$1
SCRIPTDIR=$2
BLAST_VERSION=$3
PRODUCT="ncbi-blast-$BLAST_VERSION+"

INSTALL_LOCATION1=/usr/local/ncbi/blast
INSTALL_LOCATION2=/etc/paths.d
STAGE_DIR1=_stage1
STAGE_DIR2=_stage2
RESOURCES_DIR=Resources
ID=gov.nlm.nih.ncbi.blast

if [ $# -ne 3 ] ; then
    echo "Usage: ncbi-blast.sh [installation directory] [MacOSX post-build script directory] [BLAST version]";
    exit 1;
fi

setup()
{
    rm -rf $PRODUCT.dmg $PRODUCT $STAGE_DIR1 $STAGE_DIR2 $INSTALLDIR/installer $RESOURCES_DIR
    mkdir -p $STAGE_DIR1/bin $STAGE_DIR1/doc $STAGE_DIR2 $PRODUCT
}

prep_binary_component_package() 
{
    BLAST_BINS="blastn blastp blastx tblastn tblastx psiblast rpsblast rpstblastn blast_formatter deltablast legacy_blast.pl update_blastdb.pl "
    MASKING_BINS="windowmasker dustmasker segmasker"
    DB_BINS="blastdbcmd makeblastdb makeprofiledb makembindex blastdb_aliastool convert2blastmask blastdbcheck"
    ALL_BINS="$BLAST_BINS $MASKING_BINS $DB_BINS"

    cat > $STAGE_DIR1/doc/README.txt <<EOF
The user manual is available in http://www.ncbi.nlm.nih.gov/books/NBK279690
Release notes are available in http://www.ncbi.nlm.nih.gov/books/NBK131777
EOF

    for bin in $ALL_BINS; do
        cp -p $INSTALLDIR/bin/$bin $STAGE_DIR1/bin
    done

    /usr/bin/pkgbuild --root $STAGE_DIR1 --identifier $ID.binaries --version \
        $BLAST_VERSION --install-location $INSTALL_LOCATION1 binaries.pkg
}

prep_paths_component_package()
{
    echo /usr/local/ncbi/blast/bin > $STAGE_DIR2/ncbi_blast
    /usr/bin/pkgbuild --root $STAGE_DIR2 --identifier $ID.paths --version \
        $BLAST_VERSION --install-location $INSTALL_LOCATION2 paths.pkg
}

customize_distribution_xml()
{
    sed -i.bak '/options/i\
    <title>NCBI BLAST+ Command Line Applications</title> \
    <welcome file="welcome.txt" mime-type="text/plain"/> \
    <license file="LICENSE" mime-type="text/plain"/> \
    <background scaling="proportional" alignment="left" file="large-Blue_ncbi_logo.tiff" mime-type="image/tiff"/> \
' Distribution.xml 
}

create_product_archive()
{
	/usr/bin/productbuild --synthesize --identifier $ID --version \
    $BLAST_VERSION --package binaries.pkg --package paths.pkg Distribution.xml

    customize_distribution_xml

    mkdir $RESOURCES_DIR
    cp -p $INSTALLDIR/LICENSE $RESOURCES_DIR
    for f in welcome.txt large-Blue_ncbi_logo.tiff ; do
        cp -p $SCRIPTDIR/$f $RESOURCES_DIR
    done

	/usr/bin/productbuild --resources Resources --distribution Distribution.xml $PRODUCT/$PRODUCT.pkg
    cp -p $SCRIPTDIR/uninstall_ncbi_blast.zip $PRODUCT
}

create_disk_image()
{
	/usr/bin/hdiutil create $PRODUCT.dmg -srcfolder $PRODUCT
    mkdir $INSTALLDIR/installer
    mv $PRODUCT.dmg $INSTALLDIR/installer
}

setup
prep_binary_component_package
prep_paths_component_package
create_product_archive
create_disk_image
