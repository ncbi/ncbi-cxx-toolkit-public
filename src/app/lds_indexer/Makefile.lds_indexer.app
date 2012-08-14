#################################
# $Id$
#################################

REQUIRES = objects bdb

APP = lds_indexer
SRC = lds_indexer

# new_project.sh will copy everything in the following block to any
# Makefile.*_app generated from this sample project.  Do not change
# the lines reading "### BEGIN/END COPIED SETTINGS" in any way.

### BEGIN COPIED SETTINGS
LIB = ncbi_xloader_lds lds $(OBJREAD_LIBS) bdb xobjutil $(SOBJMGR_LIBS)

LIBS = $(BERKELEYDB_LIBS) $(DL_LIBS) $(ORIG_LIBS)
### END COPIED SETTINGS

WATCHERS = vasilche
