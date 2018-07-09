#############################################################################
# $Id$
#############################################################################

NCBI_add_subdirectory(
  bam sra csra wgs vdbgraph snp
)

if(OFF)
# Include projects from this directory

# Recurse subdirectories
add_subdirectory(sra)
add_subdirectory(bam)
add_subdirectory(csra)
add_subdirectory(wgs)
add_subdirectory(vdbgraph)
add_subdirectory(snp)
endif()
