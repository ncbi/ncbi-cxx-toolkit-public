#############################################################################
# $Id$
#############################################################################

NCBI_begin_app(ncbi_dblb_cli)
  NCBI_sources(ncbi_dblb_cli)
  NCBI_uses_toolkit_libraries(sdbapi xregexp)
  NCBI_set_test_requires(connext in-house-resources)
  NCBI_set_test_assets(test_ncbi_dblb_cli.py)
  NCBI_add_test(test_ncbi_dblb_cli.sh)
NCBI_end_app()

