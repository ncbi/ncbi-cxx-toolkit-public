#################################
# $Id$
#################################

REQUIRES = dbapi MT

APP = test_objmgr1_data_mt
SRC = test_objmgr1_data_mt
LIB = xobjmgr id1 seqset $(SEQ_LIBS) pub medline biblio general dbapi_driver xser xconnect xutil xncbi test_mt

#PRE_LIBS =  -L.. -lxobjmgr1
LIBS = $(NCBI_C_LIBPATH) $(NCBI_C_ncbi) $(NETWORK_LIBS) $(ORIG_LIBS) $(DL_LIBS)

CHECK_CMD = test_objmgr1_data_mt
