#!/bin/sh
#
# $Id$
# Set some "domestic" paths, then run "configure"
#
# By Denis Vakatov (vakatov@ncbi.nlm.nih.gov)
#############################################################################

USR_BUILD_DIR="$1"
DEF_BUILD_DIR="egcs_linux"

set -a

PM_PATH="/net/neptune/pubmed1/Cvs5"
NCBI_C_INCLUDE="/home/vakatov/ncbi/include"
NCBI_C_RELEASE_LIB="/home/vakatov/ncbi/lib"
NCBI_C_DEBUG_LIB="/home/vakatov/ncbi/lib"

if test $# -gt 0; then shift ; fi
sh configure --exec_prefix=${USR_BUILD_DIR:=$DEF_BUILD_DIR} $*
