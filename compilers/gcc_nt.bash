#############################################################################
# Setup the local working environment for the "configure" script
#   Compiler:   GCC    (2.95 and higher)
#   OS:         Win-NT (4.0)
#   Processor:  Intel  (i586)
#
# $Revision$  // by Denis Vakatov, NCBI (vakatov@ncbi.nlm.nih.gov)
#############################################################################


# Export the following interpreter variables
set -a

# Shell for the "configure" script and makefiles
CONFIG_SHELL="bash"

## Configure
${CONFIG_SHELL} `dirname $0`/../configure "$@"
