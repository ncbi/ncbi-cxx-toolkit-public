APP = blast
SRC = blast_app blast_input
LIB = xblast xalnutil xalnmgr ncbi_xloader_blastdb blastdb xobjutil xobjread $(OBJMGR_LIBS)

CFLAGS   = $(FAST_CFLAGS)
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

CPPFLAGS = $(ORIG_CPPFLAGS) $(NCBI_C_INCLUDE)
LIBS = $(ORIG_LIBS) $(NCBI_C_LIBPATH) -lblastapi -lncbitool -lncbiobj $(NCBI_C_ncbi)

