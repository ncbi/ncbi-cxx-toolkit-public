#################################
# $Id$
#################################

APP = test_objmgr1_gbloader
SRC = test_objmgr1_gbloader
LIB = id1 seqset $(SEQ_LIBS) pub medline biblio general xser xconnect xutil xncbi

PRE_LIBS =  -L.. -lxobjmgr1
LIBS = $(NCBI_C_LIBPATH) $(NCBI_C_ncbi) $(NETWORK_LIBS) $(ORIG_LIBS)

CHECK_CMD = test_objmgr1_gbloader
