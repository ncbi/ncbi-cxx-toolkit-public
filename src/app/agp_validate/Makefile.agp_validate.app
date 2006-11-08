# $Id$

APP = agp_validate
SRC = agp_validate ContextValidator AccessionPatterns AgpErr LineValidator AltValidator

# ncbi_xdbapi_ftds$(STATIC) $(FTDS_LIB) dbapi$(STATIC)
LIB = entrez2cli entrez2 taxon1 xobjutil $(OBJMGR_LIBS:%=%$(STATIC))

# $(FTDS_LIBS) $(ICONV_LIBS)
LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

# FreeTDS
REQUIRES = objects dbapi

