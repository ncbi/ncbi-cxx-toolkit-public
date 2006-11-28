APP = blastall
SRC = blastall blast_format
LIB = blastinput xblastformat $(BLAST_LIBS) \
      ncbi_xloader_blastdb scoremat blastdb xalnmgr \
      xobjsimple xobjutil xobjread xhtml xcgi connect tables $(OBJMGR_LIBS)

CFLAGS   = $(FAST_CFLAGS)
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

CPPFLAGS = $(ORIG_CPPFLAGS)
LIBS = $(CMPRS_LIBS)

REQUIRES = objects C-Toolkit
