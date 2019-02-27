#############################################################################
# $Id$
#############################################################################

# Include projects from this directory
NCBI_add_app(igblastp igblastn)
NCBI_add_target(edit_imgt_file)

if(OFF)
#include(CMakeLists.igblastp.app.txt)
#include(CMakeLists.igblastn.app.txt)
#include(CMakeLists.edit_imgt_file.txt)
endif()
