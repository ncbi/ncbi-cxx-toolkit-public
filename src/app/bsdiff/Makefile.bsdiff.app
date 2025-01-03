#################################
# $Id$
# Author:  Colleen Bollin, based on file by Mati Shomrat
#################################

# Build application "biosample_chk"
#################################

APP = bsdiff
SRC = bsdiff

LIB  = xbiosample_util $(ncbi_xloader_wgs) $(SRAREAD_LIBS) xmlwrapp xvalidate \
       $(OBJEDIT_LIBS) $(XFORMAT_LIBS) xalnmgr xobjutil taxon1 valerr tables \
       xregexp $(PCRE_LIB) $(OBJMGR_LIBS)

LIBS = $(GENBANK_THIRD_PARTY_LIBS) $(LIBXSLT_LIBS) $(PCRE_LIBS) $(VDB_LIBS) \
       $(NETWORK_LIBS) $(CMPRS_LIBS) $(ORIG_LIBS)

POST_LINK = $(VDB_POST_LINK)

REQUIRES = objects $(VDB_REQ) LIBXML LIBXSLT

WATCHERS = gotvyans
