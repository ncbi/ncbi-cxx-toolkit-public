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

PM_PATH="/net/neptune/pubmed1/Cvs5"
NCBI_C_INCLUDE="$NCBI/ver0.0/ncbi/include"
NCBI_C_LIB="$NCBI/ver0.0/ncbi/altlib"

sh configure --exec_prefix=${USR_BUILD_DIR:=$DEF_BUILD_DIR}
