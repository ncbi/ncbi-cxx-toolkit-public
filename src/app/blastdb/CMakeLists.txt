#############################################################################
# $Id$
#############################################################################

NCBI_add_app(
  blastdbcmd makeblastdb blastdb_aliastool blastdbcheck convert2blastmask
  blastdbcp makeprofiledb blastdb_convert
)

if (OFF)
# Include projects from this directory
include(CMakeLists.blastdbcmd.app.txt)
include(CMakeLists.blastdb_convert.app.txt)
include(CMakeLists.makeblastdb.app.txt)
include(CMakeLists.blastdb_aliastool.app.txt)
include(CMakeLists.blastdbcheck.app.txt)
include(CMakeLists.convert2blastmask.app.txt)
include(CMakeLists.blastdbcp.app.txt)
include(CMakeLists.makeprofiledb.app.txt)
endif()
