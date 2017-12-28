##############################################################################
# 
#

include_directories(SYSTEM ${BOOST_INCLUDE})

# Include projects from this directory
include(CMakeLists.condition.app.txt)
include(CMakeLists.mutex1.app.txt)
include(CMakeLists.strings.app.txt)
include(CMakeLists.passarg.app.txt)
include(CMakeLists.strtok_r.app.txt)
