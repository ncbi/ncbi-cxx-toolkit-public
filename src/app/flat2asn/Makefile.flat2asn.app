# $Id$
#
# Makefile: flat2asn
# Author: Sema
#

APP = flat2asn
SRC = flat2asn

LIB = xflatfile xlogging taxon1 $(XFORMAT_LIBS) $(EUTILS_LIBS) $(SEQ_LIBS) pub medline biblio general xser xconnect xutil xncbi $(DATA_LOADERS_UTIL_LIB) xobjutil $(OBJMGR_LIBS) tables xregexp $(PCRE_LIB) $(DATA_LOADERS_UTIL_LIB)

LIBS = $(DATA_LOADERS_UTIL_LIBS) $(PCRE_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)


WATCHERS = foleyjp
