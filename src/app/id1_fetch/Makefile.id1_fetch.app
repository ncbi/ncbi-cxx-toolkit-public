# $Id$

APP = id1_fetch
SRC = id1_fetch
LIB = xobjutil id1 xobjmgr seqset $(SEQ_LIBS) pub medline biblio general \
      entrez2 xser xconnect xutil xncbi

LIBS = $(NCBI_C_LIBPATH) $(NCBI_C_ncbi) $(NETWORK_LIBS) $(ORIG_LIBS)

REQUIRES = objects
