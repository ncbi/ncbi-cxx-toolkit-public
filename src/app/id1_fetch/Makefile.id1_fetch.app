# $Id$

REQUIRES = objects dbapi

APP = id1_fetch
SRC = id1_fetch
LIB = xobjutil xobjmgr id1 submit seqset $(SEQ_LIBS) pub medline biblio \
      general dbapi_driver entrez2 xser xconnect xutil xncbi

LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)
