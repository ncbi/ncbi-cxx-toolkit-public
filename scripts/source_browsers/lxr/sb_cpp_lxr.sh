#! /bin/sh

# $Id$
# Author:
#    Vladimir Ivanov
#
# Build & install source browser (LXR, http://lxr.linux.no/) database
# for the NCBI C++ Toolkit on ICOREMAKE3 / FUTURAMA cluster
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
#    4 - Error SVN checkout/update source tree
#
###############################################################################

# Config path variables

# Local LXR directory
LXR=/home/coremake/lxr

# Scripts directory (for svn_core.sh)
SCRIPTS=$NCBI/c++.potluck/scripts

# C++ source trees.
# Checkout a copy from repository to specified path.
SRC_PUB=/web/public/data/ToolBox/CPP_DOC/c++
SRC_PRIV=/web/private/data/ToolBox/CPP_DOC/c++

# Building LXR indexes
BUILD_PUB=$LXR/build/internal_cpp_public
BUILD_PRIV=$LXR/build/internal_cpp_private

# Viewing tools (all web and data subdirs going web server)
VIEW_PUB="$LXR/view $LXR/view_cpp $LXR/view_cpp_public"
VIEW_PRIV="$LXR/view $LXR/view_cpp $LXR/view_cpp_private"

# Relative temporary web directories for viewers tools and generating indexes
TMP_WEB_PUB=/web/public/htdocs/IEB/ToolBox/CPP_DOC/lxr
TMP_DAT_PUB=/web/public/data/ToolBox/CPP_DOC/lxr
TMP_WEB_PRIV=/web/private/htdocs/ieb/ToolBox/CPP_DOC/lxr
TMP_DAT_PRIV=/web/private/data/ToolBox/CPP_DOC/lxr

# LXR indexes, using to view sources
WEB_BASE_PUB=$TMP_DAT_PUB/indexes
WEB_BASE_PRIV=$TMP_DAT_PRIV/indexes

# Directories to distribute content to real servers
DIST_WEB_PUB=/net/nasys02/vol/export2/tweb_pub/IEB/ToolBox/CPP_DOC/lxr
DIST_DAT_PUB=/net/nasys02/vol/export2/tweb_data_pub/ToolBox/CPP_DOC/lxr
DIST_SRC_PUB=/net/nasys02/vol/export2/tweb_data_pub/ToolBox/CPP_DOC/c++
DIST_WEB_PRIV=/net/nasys02/vol/export2/tweb_priv/ieb/ToolBox/CPP_DOC/lxr
DIST_DAT_PRIV=/net/nasys02/vol/export2/tweb_data_priv/ToolBox/CPP_DOC/lxr
DIST_SRC_PRIV=/net/nasys02/vol/export2/tweb_data_priv/ToolBox/CPP_DOC/c++


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

# Get C++ Toolkit source tree
rm -fr $SRC_PRIV  ||  exit 4
svn --non-interactive checkout https://svn.ncbi.nlm.nih.gov/repos/toolkit/trunk/c++ $SRC_PRIV  ||  exit 4

# Generating serialization code from ASN.1 specs
cd $SRC_PRIV/src/objects  ||  exit 5
    { make -f Makefile.sources builddir=$NCBI/c++.metastable/Release/build  ||  exit 5; } \
    | grep '\-m  *[a-zA-Z0-9_][a-zA-Z0-9_]*\.asn ' \
    | sed 's%^.*-m  *\([a-zA-Z0-9_][a-zA-Z0-9_]*\.asn\).*%  \1%g'
cd ../../src/app/sample/soap   ||  exit 5
    { $NCBI/c++.metastable/Release/build/new_module.sh --xsd soap_dataobj  ||  exit 5; } \
    | grep '\-m  *[a-zA-Z0-9_][a-zA-Z0-9_]*\.dtd ' \
    | sed 's%^.*-m  *\([a-zA-Z0-9_][a-zA-Z0-9_]*\.dtd\).*%  \1%g'
cd $SRC_PRIV/src/gui/objects  ||  exit 5
    for x in *.asn; do
        { $NCBI/c++.metastable/Release/build/new_module.sh `basename $x .asn`  ||  exit 5; } \
        | grep '\-m  *[a-zA-Z0-9_][a-zA-Z0-9_]*\.asn ' \
        | sed 's%^.*-m  *\([a-zA-Z0-9_][a-zA-Z0-9_]*\.asn\).*%  \1%g'
    done
cd ../dialogs/edit/feature  ||  exit 5
    { $NCBI/c++.metastable/Release/build/new_module.sh  ||  exit 5; } \
    | grep '\-m  *[a-zA-Z0-9_][a-zA-Z0-9_]*\.asn ' \
    | sed 's%^.*-m  *\([a-zA-Z0-9_][a-zA-Z0-9_]*\.asn\).*%  \1%g'

# Remove all .svn dirs in the source tree
find $SRC_PRIV -type d -name .svn -exec rm -rf {} \; >/dev/null 2>&1

# Change work directory
cd $BUILD_PRIV || exit 1
rm -f * .* >/dev/null 2>&1

# Create LXR indexes
../bin/genxref           $SRC_PRIV || exit 1
../bin/glimpseindex -H . $SRC_PRIV || exit 1
chmod -f 644 * .glimpse_*          || exit 1

# Copy all viewing tools into temporary directories also
rm   -rf $TMP_WEB_PRIV  || exit 2
rm   -rf $TMP_DAT_PRIV  || exit 2
mkdir -p $TMP_WEB_PRIV  || exit 2
mkdir -p $TMP_DAT_PRIV  || exit 2

# Agregate all viewing tools into temporary folders
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
find $TMP_WEB_PRIV -type d -name .svn -exec rm -rf {} \; >/dev/null 2>&1
find $TMP_DAT_PRIV -type d -name .svn -exec rm -rf {} \; >/dev/null 2>&1

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

# Get public C++ Toolkit source tree
rm -fr $SRC_PUB  ||  exit 4
$SCRIPTS/svn_core.sh $SRC_PUB --export --with-objects  ||  (rm -rf $SRC_PUB; exit 4)

# Delete all secure code
$SRC_PUB/scripts/internal/misc/strip_sensitive_sources.sh $SRC_PUB || (rm -rf $SRC_PUB; exit 4)

# Change work directory
cd $BUILD_PUB || exit 1
rm -f * .* >/dev/null 2>&1

# Create LXR indexes
../bin/genxref           $SRC_PUB || exit 1
../bin/glimpseindex -H . $SRC_PUB || exit 1
chmod -f 644 * .glimpse_*         || exit 1

rm   -rf $TMP_WEB_PUB  || exit 2
rm   -rf $TMP_DAT_PUB  || exit 2
mkdir -p $TMP_WEB_PUB  || exit 2
mkdir -p $TMP_DAT_PUB  || exit 2

# Aggregate all viewing tools into temporary folders
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
find $TMP_WEB_PUB  -type d -name .svn -exec rm -rf {} \; >/dev/null 2>&1
find $TMP_DAT_PUB  -type d -name .svn -exec rm -rf {} \; >/dev/null 2>&1

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
