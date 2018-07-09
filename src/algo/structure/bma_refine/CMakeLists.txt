#############################################################################
# $Id$
#############################################################################

NCBI_add_library(xbma_refiner xbma_refiner_gui)
NCBI_add_subdirectory(bma_refiner_app)

if(OFF)
# Include projects from this directory
include(CMakeLists.xbma_refiner.lib.txt)

if (${wxWidgets_FOUND})
    include(CMakeLists.xbma_refiner_gui.lib.txt)
endif()

# Recurse subdirectories
add_subdirectory(bma_refiner_app)
endif()
