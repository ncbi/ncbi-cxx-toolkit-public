#!/bin/sh
#
# $Id$
# Set some "domestic" paths, then run "configure"
#
# By Denis Vakatov (vakatov@ncbi.nlm.nih.gov)
#############################################################################

USR_BUILD_DIR="$1"
DEF_BUILD_DIR="egcs_sun"

set -a

CC="gcc"
CXX="g++"
CXXFLAGS="-fsquangle"
#AR="/netopt/SUNWspro6/bin/CC -xar -o"
PM_PATH="/net/neptune/pubmed1/Cvs5"
NCBI_C_INCLUDE=${NCBI_C_INCLUDE:="$NCBI/ver0.0/ncbi/include"}
NCBI_C_LIB=${NCBI_C_RELEASE_LIB:="$NCBI/ver0.0/ncbi/altlib"}
NCBI_C_DEBUG_LIB=${NCBI_C_DEBUG_LIB:="$NCBI/ver0.0/ncbi/altlib"}

if test $# -gt 0; then shift ; fi
sh configure --exec_prefix=${USR_BUILD_DIR:=$DEF_BUILD_DIR}
