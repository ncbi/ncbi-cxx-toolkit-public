
# This "bash" script makes some "configure" tests to work
# in the CYGWIN32_NT "bash" environment with EGC C/C++
# preprocessor/compiler/linker.
#
# NOTE:  "configure" script was produced by "autoconf" ver. 2.13,
#
# (By Denis Vakatov,  vakatov@ncbi.nlm.nih.gov)  ($Revision$)

#############################################################################


USR_BUILD_DIR="$1"
DEF_BUILD_DIR="egcs"

set -a

PM_PATH=""
NCBI_C_INCLUDE="//s/ncbi/include"
NCBI_C_LIB="//s/ncbi/foo_lib"

CONF_exe_ext=".exe"

CONFIG_SHELL="bash"
$CONFIG_SHELL ./configure --exec_prefix="${USR_BUILD_DIR:=$DEF_BUILD_DIR}"


cat << EOD
Usage:
  'bash $0 [dir]' -- configure and install to "dir"

Notes:
  "dir" is the optional destination directory name ("$DEF_BUILD_DIR" by deflt).
  You can change the path to compiler components in the very
  beginning of this("$0") script.
EOD
