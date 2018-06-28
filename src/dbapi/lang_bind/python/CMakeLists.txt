#############################################################################
# $Id$
#############################################################################

NCBI_requires(PYTHON -Cygwin)
NCBI_add_library(python_ncbi_dbapi)
NCBI_add_subdirectory(tests)

#include(CMakeLists.python_ncbi_dbapi.lib.txt)
#add_subdirectory(tests)
