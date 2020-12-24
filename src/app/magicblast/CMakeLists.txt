# $Id$

NCBI_begin_app(magicblast)
  NCBI_sources(magicblast_app magicblast_util magicblast_thread)
  NCBI_add_definitions(NCBI_MODULE=BLAST)
  NCBI_uses_toolkit_libraries(blast_sra_input blastinput)
NCBI_end_app()

