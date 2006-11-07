# $Id$

APP = dbl_lang_ftds64
SRC = ../../../ftds8/samples/ftds_lang

LIB  = ncbi_xdbapi_ftds64_dblib$(STATIC) sybdb_ftds64 tds_ftds64 dbapi_driver$(STATIC) $(XCONNEXT) xconnect xncbi
LIBS = $(FTDS64_LIBS) $(TLS_LIBS) $(ICONV_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(FTDS64_INCLUDE) $(ORIG_CPPFLAGS)
