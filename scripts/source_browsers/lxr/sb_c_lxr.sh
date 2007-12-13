#! /bin/sh

# $Id$
# Authors:  
#    Vladimir Ivanov
#
# Build & install source browser (LXR, http://lxr.linux.no/) database
# for the NCBI C Toolkit on ICOREMAKE3 / FUTURAMA cluster
#
# Using LXR:
#    http://lxr.linux.no/
#
###############################################################################
#
# Script returns the following codes:
#    0 - All right. Indexes are builded and copied into web directory.
#    1 - Error create LXR indexes.
#    2 - Error of copying viewers tools or indexes into web directory.
#    3 - Error replication data to other web-servers.
#    4 - Error CVS checkout/update C Toolkit source tree.
#
###############################################################################

# Config path variables

# Local LXR directory
LXR=/home/coremake/lxr

# Directory with scripts to prepare C Toolkit sources
SCRIPTS=/am/ncbiapdata/scripts

# Building LXR indexes
BUILD_PUB=$LXR/build/internal_c_public
BUILD_PRIV=$LXR/build/internal_c_private

# Viewing tools (all web and data subdirs going web server)
VIEW_PUB="$LXR/view $LXR/view_c $LXR/view_c_public"
VIEW_PRIV="$LXR/view $LXR/view_c $LXR/view_c_private"

# C Toolkit source tree
SRC_PUB_BASE=/web/public/data/ToolBox/C_DOC
SRC_PUB_DIR=source
SRC_PUB=$SRC_PUB_BASE/$SRC_PUB_DIR
SRC_PRIV_BASE=/web/private/data/ToolBox/C_DOC
SRC_PRIV_DIR=source
SRC_PRIV=$SRC_PRIV_BASE/$SRC_PRIV_DIR

# Relative temporary web directories for viewers tools and generating indexes
TMP_WEB_PUB=/web/public/htdocs/IEB/ToolBox/C_DOC/lxr
TMP_DAT_PUB=/web/public/data/ToolBox/C_DOC/lxr
TMP_WEB_PRIV=/web/private/htdocs/ieb/ToolBox/C_DOC/lxr
TMP_DAT_PRIV=/web/private/data/ToolBox/C_DOC/lxr

# LXR indexes, using to view sources
WEB_BASE_PUB=$TMP_DAT_PUB/indexes
WEB_BASE_PRIV=$TMP_DAT_PRIV/indexes

# Directories to distribute content to real servers
DIST_WEB_PUB=/net/nasys02/vol/export2/tweb_pub/IEB/ToolBox/C_DOC/lxr
DIST_WEB_PRIV=/net/nasys02/vol/export2/tweb_priv/ieb/ToolBox/C_DOC/lxr
DIST_DAT_PUB=/net/nasys02/vol/export2/tweb_data_pub/ToolBox/C_DOC/lxr
DIST_DAT_PRIV=/net/nasys02/vol/export2/tweb_data_priv/ToolBox/C_DOC/lxr
DIST_SRC_PUB=/net/nasys02/vol/export2/tweb_data_pub/ToolBox/C_DOC/source
DIST_SRC_PRIV=/net/nasys02/vol/export2/tweb_data_priv/ToolBox/C_DOC/source


#------------------------------------------------------------------------------

RedistDir() {
    src=$1
    dst=$2
    rm -rf $dst.new >/dev/null 2>&1
    cp -prf  $src $dst.new || exit 3
    test -d $dst  &&  (mv $dst $dst.old || exit 3)
    mv $dst.new $dst || exit 3
    test -d $dst  &&  (rm -rf $dst.old&)
}


#------------------------------------------------------------------------------

# Update LXR tools from repository
cd $LXR
svn --non-interactive update >/dev/null 2>&1


#------------------------------------------------------------------------------
# PRIVATE

# Get private C Toolkit source tree
rm -fr $SRC_PRIV
mkdir -p $SRC_PRIV                   || exit 1
cd $SRC_PRIV_BASE                    || exit 1
$SCRIPTS/cvs_ncbi_tree $SRC_PRIV_DIR || exit 1

# Change work directory
cd $BUILD_PRIV || exit 1
rm -f * .* >/dev/null 2>&1

# Create LXR indexes
../bin/genxref           $SRC_PRIV || exit 1
../bin/glimpseindex -H . $SRC_PRIV || exit 1
chmod -f 644 * .glimpse_*          || exit 1

# Copy viewing LXR tools into local temporary directories
rm   -rf $TMP_WEB_PRIV  || exit 2
rm   -rf $TMP_DAT_PRIV  || exit 2
mkdir -p $TMP_WEB_PRIV  || exit 2
mkdir -p $TMP_DAT_PRIV  || exit 2

# Copy all viewing tools into temporary directories also
for dir in $VIEW_PRIV; do
    if test -d  $dir/web ; then
       cp -fr $dir/web/* $TMP_WEB_PRIV  || exit 2
    fi
    if test -d  $dir/data ; then
       cp -fr $dir/data/* $TMP_DAT_PRIV || exit 2
    fi
done

# Copy indexes
mkdir -p $WEB_BASE_PRIV
cp -f * .glimpse_* $WEB_BASE_PRIV || exit 2

# Remove all .svn dirs
rm -rf `find $TMP_WEB_PRIV -type d -name .svn -print` >/dev/null 2>&1
rm -rf `find $TMP_DAT_PRIV -type d -name .svn -print` >/dev/null 2>&1

# Copy all data to the real webserver
RedistDir $TMP_WEB_PRIV $DIST_WEB_PRIV
RedistDir $TMP_DAT_PRIV $DIST_DAT_PRIV
RedistDir $SRC_PRIV     $DIST_SRC_PRIV

# Initiate update on public web servers
touch $DIST_WEB_PRIV/.sink_subtree
touch $DIST_DAT_PRIV/.sink_subtree
touch $DIST_SRC_PRIV/.sink_subtree


#------------------------------------------------------------------------------
# PUBLIC

# Get public C Toolkit source tree
rm -fr $SRC_PUB
cd $SRC_PUB_BASE || exit 1
doConvertDOS=0
export doConvertDOS
(echo "y" | $SCRIPTS/makedist_pc $SRC_PUB_DIR) || exit 1

# Change work directory
cd $BUILD_PUB || exit 1
rm -f * .* >/dev/null 2>&1

# Create LXR indexes
../bin/genxref           $SRC_PUB || exit 1
../bin/glimpseindex -H . $SRC_PUB || exit 1
chmod -f 644 * .glimpse_*         || exit 1

# Copy viewing LXR tools into local temporary directories
rm   -rf $TMP_WEB_PUB  || exit 2
rm   -rf $TMP_DAT_PUB  || exit 2
mkdir -p $TMP_WEB_PUB  || exit 2
mkdir -p $TMP_DAT_PUB  || exit 2

# Copy all viewing tools into temporary directories also
for dir in $VIEW_PUB; do
    if test -d  $dir/web ; then
       cp -fr $dir/web/* $TMP_WEB_PUB  || exit 2
    fi
    if test -d  $dir/data ; then
       cp -fr $dir/data/* $TMP_DAT_PUB || exit 2
    fi
done

# Copy indexes
mkdir -p $WEB_BASE_PUB
cp -f * .glimpse_* $WEB_BASE_PUB || exit 2

# Remove all .svn dirs
rm -rf `find $TMP_WEB_PUB -type d -name .svn -print` >/dev/null 2>&1
rm -rf `find $TMP_DAT_PUB -type d -name .svn -print` >/dev/null 2>&1

# Copy all data to the real webserver
RedistDir $TMP_WEB_PUB $DIST_WEB_PUB
RedistDir $TMP_DAT_PUB $DIST_DAT_PUB
RedistDir $SRC_PUB     $DIST_SRC_PUB

# Initiate update on public web servers
touch $DIST_WEB_PUB/.sink_subtree
touch $DIST_DAT_PUB/.sink_subtree
touch $DIST_SRC_PUB/.sink_subtree

#------------------------------------------------------------------------------

exit 0
