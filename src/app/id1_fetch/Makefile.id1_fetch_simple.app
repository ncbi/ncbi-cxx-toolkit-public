# $Id$

APP = id1_fetch_simple
SRC = id1_fetch_simple
LIB = id1 seqset $(SEQ_LIBS) pub medline biblio general \
      xser xconnect xutil xncbi

REQUIRES = objects

CHECK_CMD = id1_fetch_simple -gi 3
