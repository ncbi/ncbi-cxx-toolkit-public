# $Id$

APP = agp_validate
SRC = agp_validate db ContextValidator AccessionPatterns AgpErr LineValidator

LIB = entrez2cli entrez2 taxon1 ncbi_xdbapi_ftds$(STATIC) $(FTDS_LIB) \
	  dbapi$(STATIC) dbapi_driver$(STATIC) xobjutil $(OBJMGR_LIBS:%=%$(STATIC))

LIBS = $(FTDS_LIBS) $(ICONV_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) \
       $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects dbapi FreeTDS

