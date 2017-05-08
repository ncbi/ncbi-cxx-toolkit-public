##############################################################################
# 
#

# Include projects from this directory
if (USE_LOCAL_PCRE)
    include(CMakeLists.regexp.lib.txt)
    include(CMakeLists.test_pcre.app.txt)
endif()

