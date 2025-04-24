# $Id$

NCBI_add_library(blast_app_util)
NCBI_add_app(
  kblastp
  ckblastindex
  mkkblastindex
)

NCBI_project_watchers(camacho madden fongah2)
