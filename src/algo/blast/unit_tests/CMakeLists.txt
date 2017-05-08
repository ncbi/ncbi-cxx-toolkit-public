##############################################################################
# 
#

# Include projects from this directory
include_directories(SYSTEM ${BOOST_INCLUDE})

# Recurse subdirectories
add_subdirectory(blast_format)
add_subdirectory(blastdb)
add_subdirectory(seqdb_reader)
add_subdirectory(api)
