#! /bin/sh
#############################################################################
# Setup the local working environment for the "configure" script
#   Compiler:   MIPSpro 7.3 (-n32)
#   OS:         IRIX64 (6.5)
#   Processor:  mips
#
# $Revision$  // by Denis Vakatov, NCBI (vakatov@ncbi.nlm.nih.gov)
#############################################################################

## Setup for the local (NCBI) environment
set -a
NCBI_COMPILER="MIPSpro73"

CC="cc"
CXX="CC"
CXXCPP="$CXX -E -LANG:std"
LIBS="-lm"


## Configure
${CONFIG_SHELL-/bin/sh} `dirname $0`/../configure "$@"
