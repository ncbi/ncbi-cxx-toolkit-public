# $Id$

APP = ctl_lang_ftds64
SRC = ../../../ctlib/samples/ctl_lang

LIB  = ncbi_xdbapi_ftds64_ctlib$(STATIC) ct_ftds64 tds_ftds64 dbapi_driver$(STATIC) $(XCONNEXT) xconnect xncbi
LIBS = $(FTDS64_LIBS) $(TLS_LIBS) $(ICONV_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS) $(DL_LIBS)

CPPFLAGS = $(FTDS64_INCLUDE) $(ORIG_CPPFLAGS)

