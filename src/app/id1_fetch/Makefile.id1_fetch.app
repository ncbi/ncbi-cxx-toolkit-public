# $Id$

REQUIRES = objects dbapi

APP = id1_fetch
SRC = id1_fetch
LIB = xobjutil xobjmgr id1 seqset $(SEQ_LIBS) pub medline biblio general \
      dbapi_driver entrez2 xser xconnect xutil xncbi

LIBS = $(NCBI_C_LIBPATH) $(NCBI_C_ncbi) $(NETWORK_LIBS) $(ORIG_LIBS)
