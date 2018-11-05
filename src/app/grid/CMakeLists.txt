#############################################################################
# $Id$
#############################################################################

NCBI_add_subdirectory(grid_cli remote_app cgi2rcgi cgi_tunnel2grid util test)

if(OFF)
# Include projects from this directory

# Recurse subdirectories
add_subdirectory(grid_cli)
add_subdirectory(remote_app)
add_subdirectory(cgi2rcgi)
add_subdirectory(util)
add_subdirectory(test )
endif()
