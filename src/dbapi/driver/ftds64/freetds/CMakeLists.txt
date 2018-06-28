#############################################################################
# $Id$
#############################################################################

NCBI_disable_pch()
NCBI_add_subdirectory(
  tds ctlib #dblib odbc
)

# Include projects from this directory

# Recurse subdirectories
#add_subdirectory(tds)
#add_subdirectory(ctlib)
#add_subdirectory(dblib)
#add_subdirectory(odbc)
