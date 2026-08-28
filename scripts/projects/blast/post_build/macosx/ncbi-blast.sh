#*!/bin/sh -xe

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
# Number of times to retry the "hdiutil create" command before giving up.
# Can be overridden by setting HDIUTIL_CREATE_RETRIES in the environment.
HDIUTIL_CREATE_RETRIES=${HDIUTIL_CREATE_RETRIES:-5}

if [ $# -ne 3 ] ; then
    echo "Usage: ncbi-blast.sh [installation directory] [MacOSX post-build script directory] [BLAST version]";
    exit 1;
fi

setup()
{
    rm -rf $PRODUCT.dmg $PRODUCT $STAGE_DIR1 $STAGE_DIR2 $INSTALLDIR/installer $RESOURCES_DIR
    mkdir -p $STAGE_DIR1/bin $STAGE_DIR1/doc $STAGE_DIR2 $PRODUCT
}

prep_data_collection_notice_file()
{
# JIRA SB-3327 use provided BLAST_PRIVACY
  if [ ! -f ${SCRIPTDIR}/../../BLAST_PRIVACY  ] ; then
      echo "ERROR: missing: ${SCRIPTDIR}/../../BLAST_PRIVACY"
  elif [ -f ${INSTALLDIR}/BLAST_PRIVACY  ] ; then
      echo "WARNING: ${INSTALLDIR}/BLAST_PRIVACY already exists"
  else
    cp -vp ${SCRIPTDIR}/../../BLAST_PRIVACY $STAGE_DIR1/doc/BLAST_PRIVACY
    cp -vp ${SCRIPTDIR}/../../BLAST_PRIVACY $INSTALLDIR
  fi
}

prep_binary_component_package() 
{
    BLAST_BINS="blastn blastp blastx blast_usage_report tblastn tblastx psiblast rpsblast rpstblastn blast_formatter deltablast legacy_blast.pl update_blastdb.pl cleanup-blastdb-volumes.py get_species_taxids.sh"
    MASKING_BINS="windowmasker dustmasker segmasker"
    DB_BINS="blastdbcmd makeblastdb makeprofiledb makembindex makeclusterdb clusterdbcmd blastdb_aliastool convert2blastmask blastdbcheck"
    VDB_BINS="blast_formatter_vdb blast_vdb_cmd blastn_vdb tblastn_vdb"
    ALL_BINS="$BLAST_BINS $MASKING_BINS $DB_BINS $VDB_BINS"

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
    <readme file="BLAST_PRIVACY" mime-type="text/plain"/>  \
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
    cp -p $INSTALLDIR/BLAST_PRIVACY $RESOURCES_DIR
    for f in welcome.txt large-Blue_ncbi_logo.tiff ; do
        cp -p $SCRIPTDIR/$f $RESOURCES_DIR
    done

	/usr/bin/productbuild --resources Resources --distribution Distribution.xml $PRODUCT/$PRODUCT.pkg
    cp -p $SCRIPTDIR/uninstall_ncbi_blast.zip $PRODUCT
}

# Runs "hdiutil create" with the given arguments and a fresh temporary output
# path, retrying up to $HDIUTIL_CREATE_RETRIES times if it fails, waiting
# exponentially longer between each attempt (1s, 2s, 4s, ...).
retry_hdiutil_create()
{
    attempt=1
    wait_time=2
    while [ $attempt -le $HDIUTIL_CREATE_RETRIES ]; do
        DMG=$(/usr/bin/mktemp -u "${TMPDIR:-/tmp}/${PRODUCT}.XXXXXX") || return $?
        /usr/bin/hdiutil create -debug "$@" "$DMG"
        rc=$?
        if [ $rc -eq 0 ]; then
            return 0
        fi
        rm -frv "$DMG"
        if [ $attempt -eq $HDIUTIL_CREATE_RETRIES ]; then
            echo "ERROR: hdiutil create failed after $attempt attempt(s)"
            return $rc
        fi
        echo "WARNING: hdiutil create failed (attempt $attempt/$HDIUTIL_CREATE_RETRIES), retrying in ${wait_time}s..."
        sleep $wait_time
        wait_time=$((wait_time * 2))
        attempt=$((attempt + 1))
    done
}

create_disk_image()
{
    du -shc $PRODUCT    # For diagnostics
    set -x
    DMG_SIZE=$(/usr/bin/du -sm "$PRODUCT" | /usr/bin/awk '{ size = int($1 * 1.25) + 64; if (size < 512) size = 512; print size "m"; exit }')
    MOUNT_POINT=$(/usr/bin/mktemp -d "${TMPDIR:-/tmp}/${PRODUCT}.XXXXXX")
    rm -frv "$PRODUCT.dmg"
    retry_hdiutil_create \
        -size "$DMG_SIZE" \
        -format UDRW \
        -fs HFS+ \
        -volname "$PRODUCT" \
        -ov \
        -nospotlight || exit 1
    sleep 10
    /usr/bin/hdiutil attach -debug "$DMG" -mountpoint "$MOUNT_POINT" -nobrowse -owners on
    sleep 10
    /usr/bin/ditto "$PRODUCT" "$MOUNT_POINT"
    sync
    /usr/bin/hdiutil detach -debug "$MOUNT_POINT" || /usr/bin/hdiutil detach -debug "$MOUNT_POINT" -force
    /usr/bin/hdiutil convert -debug "$DMG" -format UDZO -o "$PRODUCT.dmg"
    rm -frv "$DMG"
    mkdir $INSTALLDIR/installer
    mv $PRODUCT.dmg $INSTALLDIR/installer
}

setup
prep_data_collection_notice_file
prep_binary_component_package
prep_paths_component_package
create_product_archive
create_disk_image
