#################################
# $Id$
#################################

REQUIRES = objects dbapi

APP = lds_sample
SRC = lds_sample

REQUIRES = bdb

# new_project.sh will copy everything in the following block to any
# Makefile.*_app generated from this sample project.  Do not change
# the lines reading "### BEGIN/END COPIED SETTINGS" in any way.

### BEGIN COPIED SETTINGS
LIB = lds lds_admin xobjmgr xobjread bdb xutil xobjutil ncbi_xloader_lds $(OBJMGR_LIBS)

LIBS = $(BERKELEYDB_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)
### END COPIED SETTINGS
