# $Id$
#
# Makefile: flat2asn
# Author: Sema
#

APP = flat2asn
SRC = flat2asn

LIB = xflatfile xlogging taxon1 $(XFORMAT_LIBS) $(DATA_LOADERS_UTIL_LIB) xobjutil $(OBJMGR_LIBS) tables xregexp $(PCRE_LIB) $(DATA_LOADERS_UTIL_LIB)

LIBS = $(DATA_LOADERS_UTIL_LIBS) $(PCRE_LIBS) $(ORIG_LIBS)

#CPPFLAGS= $(LIBXML_INCLUDE) $(LIBXSLT_INCLUDE) $(ORIG_CPPFLAGS) -I$(import_root)/../include

WATCHERS = kachalos foleyjp
