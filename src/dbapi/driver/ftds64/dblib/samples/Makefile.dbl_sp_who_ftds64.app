# $Id$

APP = dbl_sp_who_ftds64
SRC = dbl_sp_who_ftds64

LIB  = ncbi_xdbapi_ftds64_dblib$(STATIC) sybdb_ftds64 tds_ftds64 dbapi_driver$(STATIC) $(XCONNEXT) xconnect xncbi
LIBS = $(TLS_LIBS) $(ICONV_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS) $(DL_LIBS)

CPPFLAGS = $(FTDS64_INCLUDE) $(ORIG_CPPFLAGS)

