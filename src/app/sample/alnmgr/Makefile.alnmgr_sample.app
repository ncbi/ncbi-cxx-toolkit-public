#################################
# $Id$
#################################

REQUIRES = objects dbapi

APP = alnmgr_sample
SRC = alnmgr_sample

# new_project.sh will copy everything in the following block to any
# Makefile.*_app generated from this sample project.  Do not change
# the lines reading "### BEGIN/END COPIED SETTINGS" in any way.

### BEGIN COPIED SETTINGS
LIB = xalnmgr xobjmgr id1 seqset $(SEQ_LIBS) pub medline biblio general \
      dbapi_driver xser xutil xconnect xncbi
LIBS = $(DL_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)
### END COPIED SETTINGS
