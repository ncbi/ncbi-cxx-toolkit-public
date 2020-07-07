# $Id$
#
# Makefile: flat2asn
# Author: Sema
#

APP = flat2asn
SRC = flat2asn

LIB = xflatfile ctransition taxon1 $(OBJMGR_LIBS) $(XFORMAT_LIBS)

LIBS = $(DATA_LOADERS_UTIL_LIBS) $(PCRE_LIBS)

#CPPFLAGS= $(LIBXML_INCLUDE) $(LIBXSLT_INCLUDE) $(ORIG_CPPFLAGS) -I$(import_root)/../include

WATCHERS = kachalos
