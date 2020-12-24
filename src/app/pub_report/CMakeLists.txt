# $Id$

NCBI_begin_app(pub_report)
  NCBI_sources(
    journal_hook journal_report pub_report seq_entry_hook seqid_hook unpub_hook
    unpub_report utils
  )
  NCBI_uses_toolkit_libraries(xvalidate eutils_client hydra_client)
NCBI_end_app()
