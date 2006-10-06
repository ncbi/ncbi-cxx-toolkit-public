APP = cobalt
SRC = cobalt_app
LIB = cobalt xalgoalignnw xalgophytree fastme biotree ncbi_xloader_blastdb \
      xalnmgr $(BLAST_LIBS) $(OBJMGR_LIBS)

CFLAGS   = $(FAST_CFLAGS)
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

# CPPFLAGS = $(ORIG_CPPFLAGS) $(NCBI_C_INCLUDE)
LIBS = $(NETWORK_LIBS) $(CMPRS_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = -Cygwin
