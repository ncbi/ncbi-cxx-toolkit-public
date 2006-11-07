# $Id$

APP = odbc_lang_ftds64
SRC = ../../../odbc/samples/odbc_lang

LIB  = ncbi_xdbapi_ftds64_odbc$(STATIC) odbc_ftds64 tds_ftds64 dbapi_driver$(STATIC) $(XCONNEXT) xconnect xncbi
LIBS = $(FTDS64_LIBS) $(TLS_LIBS) $(ICONV_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(FTDS64_INCLUDE) $(ORIG_CPPFLAGS)

