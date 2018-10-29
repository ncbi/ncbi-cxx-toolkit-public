#############################################################################
# $Id$
#############################################################################

NCBI_project_watchers(camacho madden fongah2)
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

if(OFF)
# Include projects from this directory
include(CMakeLists.blast_app_util.lib.txt)
include(CMakeLists.blastp.app.txt)
include(CMakeLists.blastn.app.txt)
include(CMakeLists.blastx.app.txt)
include(CMakeLists.tblastn.app.txt)
include(CMakeLists.tblastx.app.txt)
include(CMakeLists.psiblast.app.txt)
include(CMakeLists.rpsblast.app.txt)
include(CMakeLists.rpstblastn.app.txt)
include(CMakeLists.blast_formatter.app.txt)
include(CMakeLists.deltablast.app.txt)
include(CMakeLists.seedtop.app.txt)
include(CMakeLists.legacy_blast.txt)
include(CMakeLists.update_blastdb.txt)
endif()
