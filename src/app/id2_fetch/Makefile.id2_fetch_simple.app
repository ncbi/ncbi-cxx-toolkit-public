# $Id$

APP = id2_fetch_simple
SRC = id2_fetch_simple
LIB = id2 seqsplit seqset $(SEQ_LIBS) pub medline biblio general \
      xser xconnect xutil xncbi

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)

REQUIRES = objects

# CHECK_CMD = id2_fetch_simple -gi 3
