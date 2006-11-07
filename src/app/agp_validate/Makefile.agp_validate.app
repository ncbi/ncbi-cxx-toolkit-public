# $Id$

APP = agp_validate
SRC = agp_validate ContextValidator AccessionPatterns AgpErr LineValidator AltValidator

LIB = entrez2cli entrez2 taxon1 ncbi_xdbapi_ftds$(STATIC) $(FTDS_LIB) \
	  dbapi$(STATIC) dbapi_driver$(STATIC) xobjutil $(OBJMGR_LIBS:%=%$(STATIC))

# $(FTDS_LIBS)
LIBS = $(ICONV_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) \
       $(DL_LIBS) $(ORIG_LIBS)

#  dbapi FreeTDS
REQUIRES = objects

