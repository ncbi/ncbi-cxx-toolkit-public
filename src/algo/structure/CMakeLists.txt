#############################################################################
# $Id$
#############################################################################

NCBI_add_subdirectory(struct_dp struct_util cd_utils wx_tools bma_refine threader)

if(OFF)

# Recurse subdirectories
add_subdirectory(struct_dp)
add_subdirectory(struct_util)
add_subdirectory(cd_utils)
add_subdirectory(wx_tools)
add_subdirectory(bma_refine)
add_subdirectory(threader)
endif()
