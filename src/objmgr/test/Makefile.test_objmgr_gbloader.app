#################################
# $Id$
#################################

REQUIRES = dbapi

APP = test_objmgr1_gbloader
SRC = test_objmgr1_gbloader
LIB = xobjutil xobjmgr id1 submit seqset $(SEQ_LIBS) pub medline biblio \
      general dbapi_driver xser xutil xconnect xncbi

#PRE_LIBS =  -L.. -lxobjmgr1
LIBS = $(NCBI_C_LIBPATH) $(NCBI_C_ncbi) $(NETWORK_LIBS) $(ORIG_LIBS) $(DL_LIBS)

#CHECK_CMD = test_objmgr1_gbloader
