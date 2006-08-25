# $Id$

APP = agp_validate
SRC = agp_validate db

LIB = entrez2cli entrez2 taxon1 ncbi_xdbapi_ftds-static $(FTDS_LIB) \
	  dbapi-static dbapi_driver xobjutil $(OBJMGR_LIBS:%=%$(STATIC))

LIBS = $(FTDS_LIBS:%=%$(STATIC)) $(ICONV_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) \
       $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects dbapi FreeTDS

