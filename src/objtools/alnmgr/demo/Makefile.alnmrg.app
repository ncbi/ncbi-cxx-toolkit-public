# $Id$

REQUIRES = dbapi

APP = alnmrg
SRC = alnmrg
LIB = xalnmgr xobjmgr id1 seqset $(SEQ_LIBS) pub medline biblio general \
      dbapi_driver xser xutil xconnect xncbi
LIBS = $(DL_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)
