# $Id$


REQUIRES = dbapi

APP = reader_id1_test
SRC = reader_id1_test
LIB = xobjmgr id1 seqset $(SEQ_LIBS) pub medline biblio general dbapi_driver xser xconnect xutil xncbi

LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

