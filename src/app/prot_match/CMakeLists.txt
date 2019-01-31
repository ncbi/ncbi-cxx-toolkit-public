#############################################################################
# $Id$
#############################################################################

NCBI_begin_app(prot_match)
  NCBI_sources(prot_match run_binary)
  NCBI_uses_toolkit_libraries(protein_match data_loaders_util)
NCBI_end_app()

NCBI_begin_app(update_prot_id)
  NCBI_sources(update_prot_id)
  NCBI_uses_toolkit_libraries(variation_utils)
NCBI_end_app()

