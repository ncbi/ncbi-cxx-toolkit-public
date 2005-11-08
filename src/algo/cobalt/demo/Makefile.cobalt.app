APP = cobalt
SRC = cobalt_app
LIB = cobalt xalgoalignnw xalgophytree fastme biotree xblast xalgodustmask \
      xnetblastcli xnetblast scoremat ncbi_xloader_blastdb seqdb blastdb \
      xalnmgr xobjutil xobjread tables xobjmgr seqset seq seqcode sequtil \
      pub medline biblio general xser xutil xncbi

CFLAGS   = $(FAST_CFLAGS)
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

CPPFLAGS = $(ORIG_CPPFLAGS) $(NCBI_C_INCLUDE)
LIBS = $(NCBI_C_LIBPATH) $(NCBI_C_ncbi) $(DL_LIBS) $(ORIG_LIBS)

