#############################################################################
# Setup the local working environment for the "configure" script
# ATTENTION:  Use BASH(only!) to launch:  bash compilers/msvc_nt.bash
#
#   Compiler:   MSVC++ (6.0 SP3)
#   OS:         Win-NT (4.0)
#   Processor:  Intel  (i586)
#
# $Revision$  // by Denis Vakatov, NCBI (vakatov@ncbi.nlm.nih.gov)
#############################################################################


# Path to the MSVC++ package
MSVC_DRIVE="D"
MSVC_PATH="Program Files/Microsoft Visual Studio"
PATH_DOS="$MSVC_DRIVE:\\\\`echo $MSVC_PATH | sed 's%/%\\\\\\\\%g'`"
PATH_BSH="//$MSVC_DRIVE/$MSVC_PATH"

# Export the following interpreter variables
set -a
NCBI_COMPILER="MSVC"

# Shell for the "configure" script and makefiles
CONFIG_SHELL="bash"

# MSVC working environment
LIB="$PATH_DOS\\VC98\\lib"
INCLUDE="$PATH_DOS\\VC98\\include"

# Path
PATH="$PATH_BSH/Common/MSDev98/Bin:$PATH_BSH/VC98/Bin:$PATH"

# Compiler
CC="cl.exe /TC"
CXX="cl.exe /TP"

## Configure
${CONFIG_SHELL} `dirname $0`/../configure "$@"
