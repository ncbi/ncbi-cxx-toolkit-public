#################################
# $Id$
# Author:  Eugene Vasilchenko (vasilche@ncbi.nlm.nih.gov)
#################################

# Build object manager test application "testobjmgr"
#################################

APP = testobjmgr1
SRC = testobjmgr1
LIB = xobjmgr1 id1 seqset $(SEQ_LIBS) pub medline biblio general \
      xser xutil xconnect xncbi

LIBS = $(NCBI_C_LIBPATH) $(NCBI_C_ncbi) $(NETWORK_LIBS) $(ORIG_LIBS)

CHECK_CMD = testobjmgr1
