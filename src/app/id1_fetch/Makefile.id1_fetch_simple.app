# $Id$

APP = id1_fetch_simple
OBJ = id1_fetch_simple
LIB = id1 seqset $(SEQ_LIBS) pub medline biblio general \
      xser xconnect xutil xncbi

LIBS = $(NCBI_C_LIBPATH) $(NCBI_C_ncbi) $(NETWORK_LIBS) $(ORIG_LIBS)

REQUIRES = objects

CHECK_CMD = id1_fetch_simple -gi 3
