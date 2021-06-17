# $Id$

NCBI_project_tags(test)
NCBI_requires(Boost.Test.Included)
NCBI_add_app(
  ecnum_unit_test
  mapper_unit_test
  seq_c_compat_unit_test
  seq_id_unit_test
  seq_loc_unit_test
  seqalign_unit_test 
  seqfeat_unit_test 
  seqfeatdata_unit_test
  unit_test_so_map
  unit_test_linkage_evidence
)
