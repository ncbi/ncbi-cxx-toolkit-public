# $Id$

APP = id1_fetch_simple
OBJ = id1_fetch_simple
LIB = id1 seqset $(SEQ_LIBS) pub medline biblio general \
      xser xutil xconnect xncbi

LIBS = $(NCBI_C_LIBPATH) $(NCBI_C_ncbi) $(NETWORK_LIBS) $(ORIG_LIBS)
