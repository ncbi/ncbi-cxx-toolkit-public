#################################
# $Id$
#################################

APP = test_objmgr1_basic
SRC = test_objmgr1_basic
LIB = xobjmgr1 id1 seqset $(SEQ_LIBS) pub medline biblio general \
      xser xutil xconnect xncbi

LIBS = $(NCBI_C_LIBPATH) $(NCBI_C_ncbi) $(NETWORK_LIBS) $(ORIG_LIBS)

CHECK_CMD = test_objmgr1_basic
