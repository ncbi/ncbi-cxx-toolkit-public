# $Id$

APP = id1_fetch
OBJ = id1_fetch genbank util
LIB = id1 entrez2 seqset seq seqres seqloc seqalign seqfeat seqblock \
      pub medline biblio seqcode general \
      xser xobjmgr xutil xconnect xncbi

LIBS = $(NCBI_C_LIBPATH) $(NCBI_C_ncbi) $(NETWORK_LIBS) $(ORIG_LIBS)
