##############################################################################
# 
#

# Include projects from this directory
include(CMakeLists.cgi.lib.txt)
if (FastCGI_FOUND)
    include(CMakeLists.fcgi.lib.txt)
endif()

# Recurse subdirectories
add_subdirectory(test )
