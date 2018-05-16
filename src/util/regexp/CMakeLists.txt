#############################################################################
# $Id$
#############################################################################
# 
#

# Include projects from this directory
if (ON)
NCBI_disable_pch()
NCBI_add_library(regexp)
NCBI_add_app(test_pcre)

else()
if (USE_LOCAL_PCRE)
    include(CMakeLists.regexp.lib.txt)
    include(CMakeLists.test_pcre.app.txt)
endif()
endif()

