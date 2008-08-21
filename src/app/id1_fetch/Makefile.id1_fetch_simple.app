# $Id$

APP = id1_fetch_simple
SRC = id1_fetch_simple
LIB = id1 seqset $(SEQ_LIBS) pub medline biblio general \
      xser xconnect xutil xncbi

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)

REQUIRES = objects

CHECK_CMD = id1_fetch_simple -gi 3 /CHECK_NAME=id1_fetch_simple
