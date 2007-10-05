#! /bin/sh
#############################################################################
# Setup the local working environment for the "configure" script
#   Compiler:    Sun C++ 5.9 (Studio 12)
#   OS:          Solaris
#   Processors:  Sparc,  Intel
#
# $Revision$  // by Denis Vakatov, NCBI (vakatov@ncbi.nlm.nih.gov)
#############################################################################


## Compiler location and attributes
WS_BIN="/netopt/studio12/SUNWspro/bin"
export WS_BIN

## Configure using generic script "WorkShop.sh"
${CONFIG_SHELL-/bin/sh} `dirname $0`/WorkShop.sh "$@"
