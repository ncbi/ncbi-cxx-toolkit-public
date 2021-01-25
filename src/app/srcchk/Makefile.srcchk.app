#################################
# $Id$
# Author: Frank Ludwig
#################################

# Build application "srcchk"
#################################

APP = srcchk
SRC = srcchk
LIB = xobjwrite $(XFORMAT_LIBS) variation_utils $(OBJREAD_LIBS) xalnmgr \
      xobjutil entrez2cli entrez2 tables xregexp $(PCRE_LIB) $(DATA_LOADERS_UTIL_LIB) $(OBJMGR_LIBS)

LIBS = $(GENBANK_THIRD_PARTY_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects -Cygwin

WATCHERS = ludwigf foleyjp
