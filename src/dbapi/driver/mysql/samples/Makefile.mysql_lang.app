# $Id$

APP = mysql_lang
SRC = mysql_lang

LIB  = ncbi_xdbapi_mysql dbapi_driver xncbi

LIBS = $(MYSQL_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)
CPPFLAGS = $(MYSQL_INCLUDE) $(ORIG_CPPFLAGS)

