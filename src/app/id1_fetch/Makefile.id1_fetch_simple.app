# $Id$

APP = id1_fetch_simple
OBJ = id1_fetch_simple
LIB = id1 seqset seq seqres seqloc seqalign seqfeat seqblock \
      pub medline biblio seqcode general \
      xser  xutil xconnect xncbi

LIBS = $(NCBI_C_LIBPATH) $(NCBI_C_ncbi) $(NETWORK_LIBS) $(ORIG_LIBS)
