#################################
# $Id$
#################################

REQUIRES = objects dbapi

APP = objmgr_sample
SRC = objmgr_sample

# new_project.sh will copy everything in the following block to any
# Makefile.*_app generated from this sample project.  Do not change
# the lines reading "### BEGIN/END COPIED SETTINGS" in any way.

### BEGIN COPIED SETTINGS
LIB = xobjmgr id1 seqset $(SEQ_LIBS) pub medline biblio general \
      dbapi_driver xser xutil xconnect xncbi
# The serialization code needs the C Toolkit corelib, but only if it's present.
LIBS = $(NCBI_C_LIBPATH) $(NCBI_C_ncbi) $(DL_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)
### END COPIED SETTINGS
