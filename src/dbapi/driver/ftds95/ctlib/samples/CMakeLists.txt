##############################################################################
# 
#

add_definitions(-DFTDS_IN_USE)
include_directories(SYSTEM ${includedir}/dbapi/driver/ftds95)

# Include projects from this directory
include(CMakeLists.ctl_sp_who_ftds95.app.txt)
include(CMakeLists.ctl_lang_ftds95.app.txt)

