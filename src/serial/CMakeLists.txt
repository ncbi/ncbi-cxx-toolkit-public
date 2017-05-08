##############################################################################
# 
#

# Include projects from this directory
include(CMakeLists.serial.lib.txt)
include(CMakeLists.cserial.lib.txt)

# Recurse subdirectories
add_subdirectory(datatool)
add_subdirectory(test )
add_subdirectory(soap)
