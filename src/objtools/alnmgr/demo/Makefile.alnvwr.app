# $Id$

REQUIRES = dbapi

APP = alnvwr
SRC = alnvwr
LIB = xalnmgr xobjmgr id1 seqset $(SEQ_LIBS) pub medline biblio general \
      dbapi_driver xser xutil xconnect xncbi
LIBS = $(DL_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)
