#!/bin/sh
#
# $Id$
# Set some "domestic" paths, then run "configure"
#
# By Denis Vakatov (vakatov@ncbi.nlm.nih.gov)
#############################################################################

USR_BUILD_DIR="$1"
DEF_BUILD_DIR="egcs_irix64"

set -a

CC="gcc"
CXX="g++"
CXXFLAGS="-fsquangle"
PM_PATH=${PM_PATH:="/net/neptune/pubmed1/Cvs5"}
NCBI_C_INCLUDE=${NCBI_C_INCLUDE:="$NCBI/include/NCBI"}
NCBI_C_RELEASE_LIB=${NCBI_C_RELEASE_LIB:="$NCBI/lib"}
NCBI_C_DEBUG_LIB=${NCBI_C_DEBUG_LIB:="$NCBI/altlib"}

if test $# -gt 0; then shift ; fi
sh configure --exec_prefix=${USR_BUILD_DIR:=$DEF_BUILD_DIR} $*
