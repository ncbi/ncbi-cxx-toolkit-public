#############################################################################
# $Id$
#############################################################################

NCBI_add_app(
  asn_cache_test cache_index_copy concat_seqentries
  dump_seqids prime_cache read_index_speed
  sub_cache_create walk_cache_test
)

if(OFF)
include(CMakeLists.asn_cache_test.app.txt)
include(CMakeLists.concat_seqentries.app.txt)
include(CMakeLists.read_index_speed.app.txt)
include(CMakeLists.walk_cache_test.app.txt)
include(CMakeLists.cache_index_copy.app.txt)
include(CMakeLists.dump_seqids.app.txt)
include(CMakeLists.prime_cache.app.txt)
include(CMakeLists.sub_cache_create.app.txt)
endif()
