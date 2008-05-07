# $Id$
#################################
# Build demo "compart"

APP = compart
SRC = compart em

LIB =  xalgoalignutil ncbi_xloader_blastdb xalnmgr \
       $(BLAST_LIBS:%=%$(STATIC)) \
       $(OBJMGR_LIBS:%=%$(STATIC))

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

REQUIRES = -Cygwin
