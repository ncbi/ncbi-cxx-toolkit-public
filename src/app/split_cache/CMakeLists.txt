# $Id$

NCBI_begin_app(split_cache)

  NCBI_sources(split_cache)
  NCBI_uses_toolkit_libraries(id2_split ncbi_xloader_genbank ncbi_xcache_bdb)

  NCBI_set_test_timeout(1000)
  NCBI_set_test_assets(test_split_cache.sh)
  NCBI_add_test(test_split_cache.sh)

NCBI_end_app()
