# $Id$

APP = test_id1_client
SRC = test_id1_client
LIB = id1cli id1 seqset $(SEQ_LIBS) pub medline biblio general \
      xser xconnect xutil xncbi

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)

WATCHERS = ucko
