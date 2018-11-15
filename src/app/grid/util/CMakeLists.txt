#############################################################################
# $Id$
#############################################################################

NCBI_add_app(ncfetch cidtool)

if (OFF)
# Include projects from this directory
include(CMakeLists.ncfetch.app.txt)
include(CMakeLists.cidtool.app.txt)
endif()
