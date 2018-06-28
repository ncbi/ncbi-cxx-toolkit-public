#############################################################################
# $Id$
#############################################################################

NCBI_add_subdirectory(
  unit_test_util readers blast lds2 data_loaders simple
  alnmgr cddalignview test manip cleanup format edit validator
  asniotest align seqmasks_io eutils
  align_format snputil uudutil variation writers #pubseq_gateway
)
if(OFF)
# Include projects from this directory

# Recurse subdirectories
add_subdirectory(unit_test_util )
add_subdirectory(readers)
add_subdirectory(blast)
add_subdirectory(lds2)
add_subdirectory(data_loaders)
add_subdirectory(simple)
add_subdirectory(alnmgr)
add_subdirectory(cddalignview)
add_subdirectory(test )
add_subdirectory(manip)
add_subdirectory(cleanup)
add_subdirectory(format)
add_subdirectory(edit)
add_subdirectory(validator)
add_subdirectory(asniotest)
add_subdirectory(align)
add_subdirectory(seqmasks_io)
add_subdirectory(eutils)
add_subdirectory(align_format)
add_subdirectory(snputil)
add_subdirectory(uudutil)
add_subdirectory(variation)
add_subdirectory(writers)
endif()
