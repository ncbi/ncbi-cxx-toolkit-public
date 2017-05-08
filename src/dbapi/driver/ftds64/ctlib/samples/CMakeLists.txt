##############################################################################
# 
#

add_definitions(-DFTDS_IN_USE)
include_directories(SYSTEM ${includedir}/dbapi/driver/ftds64)

# Include projects from this directory
include(CMakeLists.ctl_sp_who_ftds64.app.txt)
include(CMakeLists.ctl_lang_ftds64.app.txt)

