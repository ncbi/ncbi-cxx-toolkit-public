#################################
# $Id$
# Author:  Aleksey Grichenko (grichenk@ncbi.nlm.nih.gov)
#################################

# Build object manager demo application "objmgr_demo"
#################################

APP = objmgr_demo
SRC = objmgr_demo
LIB = xobjmgr1 id1 seqset $(SEQ_LIBS) pub medline biblio general \
      xser xutil xconnect xncbi

LIBS = $(NCBI_C_LIBPATH) $(NCBI_C_ncbi) $(NETWORK_LIBS) $(ORIG_LIBS)

CHECK_CMD = objmgr_demo
