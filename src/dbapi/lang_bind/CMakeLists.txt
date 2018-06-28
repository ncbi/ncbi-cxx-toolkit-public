#############################################################################
# $Id$
#############################################################################


NCBI_add_subdirectory(python)

if(OFF)
# Include projects from this directory
if (${PYTHON_FOUND})
    add_subdirectory(python)
endif()
endif()
