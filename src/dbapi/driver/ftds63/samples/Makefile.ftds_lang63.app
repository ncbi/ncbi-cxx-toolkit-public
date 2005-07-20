# $Id$

APP = ftds_lang63
SRC = ftds_lang

LIB  = ncbi_xdbapi_$(ftds63) $(FTDS63_LIB) dbapi_driver xncbi
LIBS = $(FTDS63_LIBS) $(ICONV_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(FTDS63_INCLUDE) $(ORIG_CPPFLAGS)

REQUIRES = FreeTDS

CHECK_CMD =
