#############################################################################
# $Id$
#############################################################################

NCBI_add_subdirectory(
  seqdb_reader seqdb_writer gene_info_reader gene_info_writer services blastdb_format
)
if(OFF)
# Include projects from this directory

# Recurse subdirectories
add_subdirectory(seqdb_reader)
add_subdirectory(seqdb_writer)
add_subdirectory(gene_info_reader)
add_subdirectory(gene_info_writer)
add_subdirectory(services)
add_subdirectory(blastdb_format)
endif()
