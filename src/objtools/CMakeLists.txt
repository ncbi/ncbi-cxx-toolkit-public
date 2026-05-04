NCBI_add_subdirectory(
  unit_test_util readers blast lds2 data_loaders simple
  alnmgr cddalignview test manip cleanup format edit 
  huge_asn validator
  asniotest align seqmasks_io eutils
  align_format snputil uudutil variation writers pubseq_gateway
  logging import
)
if(NOT NCBI_SUBUTILS_REPO)
  NCBI_add_subdirectory(flatfile)
endif()
