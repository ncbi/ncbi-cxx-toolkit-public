# $Id$

APP = id1_fetch
OBJ = id1_fetch
LIB = xobjutil id1 entrez2 seqset $(SEQ_LIBS) pub medline biblio general \
      xser xobjmgr xutil xconnect xncbi

LIBS = $(NCBI_C_LIBPATH) $(NCBI_C_ncbi) $(NETWORK_LIBS) $(ORIG_LIBS)
