# $Id$

APP = dbl_sp_who_ftds64
SRC = ../../../ftds8/samples/ftds_sp_who

LIB  = ncbi_xdbapi_ftds64_dblib$(STATIC) sybdb_ftds64 tds_ftds64 dbapi_driver$(STATIC) $(XCONNEXT) xconnect xncbi
LIBS = $(FTDS64_LIBS) $(TLS_LIBS) $(ICONV_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS) $(DL_LIBS)

CPPFLAGS = $(FTDS64_INCLUDE) $(ORIG_CPPFLAGS)

