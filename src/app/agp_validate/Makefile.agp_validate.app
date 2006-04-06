# $Id$

APP = agp_validate
SRC = agp_validate db

LIB = entrez2cli entrez2 taxon1 ncbi_xdbapi_ftds $(FTDS_LIBS) \
	  dbapi xobjutil $(OBJMGR_LIBS)

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

