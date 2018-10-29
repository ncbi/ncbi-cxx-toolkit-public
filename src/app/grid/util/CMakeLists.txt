#############################################################################
# $Id$
#############################################################################

NCBI_add_app(netcache_control ncfetch netschedule_control cidtool)

if (OFF)
# Include projects from this directory
include(CMakeLists.netcache_control.app.txt)
include(CMakeLists.ncfetch.app.txt)
include(CMakeLists.netschedule_control.app.txt)
include(CMakeLists.cidtool.app.txt)
endif()
