#################################
# $Id$
#################################

APP = splign
SRC = splign_app

LIB = xalgoalignsplign xalgoalignutil xalgoalignnw xalgoseq \
      ncbi_xloader_blastdb ncbi_xloader_lds lds bdb \
      xblast composition_adjustment xalgodustmask xnetblastcli \
      seqdb  blastdb \
      xnetblast scoremat xobjsimple xalnmgr xobjutil xobjread tables \
      $(OBJMGR_LIBS:%=%$(STATIC))
#      $(OBJMGR_LIBS)

LIBS = $(BERKELEYDB_STATIC_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) \
       $(DL_LIBS) $(ORIG_LIBS)

CXXFLAGS = $(FAST_CXXFLAGS)
#CXXFLAGS = $(FAST_CXXFLAGS) -D GENOME_PIPELINE
LDFLAGS  = $(FAST_LDFLAGS)

REQUIRES = BerkeleyDB dbapi objects -Cygwin
