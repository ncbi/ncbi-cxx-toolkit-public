#################################
# $Id$
# Author:  Colleen Bollin, based on file by Mati Shomrat
#################################

# Build application "asnval"
#################################

APP = asnvalidate
SRC = asnval
LIB = xvalidate xmlwrapp $(OBJEDIT_LIBS) $(XFORMAT_LIBS) xalnmgr xobjutil \
      valerr tables xregexp $(PCRE_LIB) $(DATA_LOADERS_UTIL_LIB) $(OBJMGR_LIBS)
LIBS = $(LIBXSLT_LIBS) $(DATA_LOADERS_UTIL_LIBS) $(LIBXML_LIBS) $(PCRE_LIBS) \
       $(CMPRS_LIBS) $(ORIG_LIBS)

POST_LINK = $(VDB_POST_LINK)

REQUIRES = objects LIBXML LIBXSLT

CXXFLAGS += $(ORIG_CXXFLAGS)
LDFLAGS  += $(ORIG_LDFLAGS)

WATCHERS = bollin gotvyans
