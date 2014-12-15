# $Id$

APP = ncbi_dblb_cli
SRC = ncbi_dblb_cli

LIB  = $(SDBAPI_LIB) xconnect xregexp $(PCRE_LIB) xutil xncbi
LIBS = $(SDBAPI_LIBS) $(PCRE_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)
