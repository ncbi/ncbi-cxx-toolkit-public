##############################################################################
# 
#

# Include projects from this directory

# Recurse subdirectories
if (BZIP2_INTERNAL)
    add_subdirectory(bzip2)
endif()

if (ZLIB_INTERNAL)
    add_subdirectory(zlib)
endif()
add_subdirectory(api)
