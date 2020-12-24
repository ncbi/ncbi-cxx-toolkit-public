# $Id$

NCBI_add_library(blast_app_util)
NCBI_add_app(
  blastp
  blastn
  blastx
  tblastn
  tblastx
  psiblast
  rpsblast
  rpstblastn
  blast_formatter
  blast_report
  deltablast
  seedtop
)

NCBI_project_watchers(camacho madden fongah2)
