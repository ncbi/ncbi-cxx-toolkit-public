#############################################################################
# Setup the local working environment for the "configure" script
#   Compiler:   MSVC++ (6.0 SP3)
#   OS:         Win-NT (4.0)
#   Processor:  Intel  (i586)
#
# $Revision$  // by Denis Vakatov, NCBI (vakatov@ncbi.nlm.nih.gov)
#############################################################################


# Path to the MSVC++ package
MSVC_DRIVE="D"
MSVC_PATH="Program Files/Microsoft Visual Studio"

# Shell for the "configure" script and makefiles
CONFIG_SHELL="bash"

# Working environment
PATH_BSH="//$MSVC_DRIVE/$MSVC_PATH"
LIB="$PATH_DOS\\VC98\\lib"
INCLUDE="$PATH_DOS\\VC98\\include"

# Path
PATH_DOS="$MSVC_DRIVE:\\\\`echo $MSVC_PATH | sed 's%/%\\\\\\\\%g'`"
PATH="$PATH_BSH/Common/MSDev98/Bin:$PATH_BSH/VC98/Bin:$PATH"

# Compiler
CC="cl.exe /TC"
CXX="cl.exe /TP"
