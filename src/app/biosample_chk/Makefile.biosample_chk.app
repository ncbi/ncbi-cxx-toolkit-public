#################################
# $Id$
# Author:  Colleen Bollin, based on file by Mati Shomrat
#################################

# Build application "biosample_chk"
#################################

APP = biosample_chk
SRC = biosample_chk

LIB  = xbiosample_util ncbi_xloader_wgs $(SRAREAD_LIBS) xmlwrapp xvalidate \
       $(OBJEDIT_LIBS) $(XFORMAT_LIBS) xalnmgr xobjutil valerr tables \
       xregexp $(PCRE_LIB) $(OBJMGR_LIBS) LIBS = $(LIBXSLT_LIBS) $(PCRE_LIBS) $(VDB_LIBS) \
       $(NETWORK_LIBS) $(CMPRS_LIBS) $(ORIG_LIBS)

LIBS = $(PCRE_LIBS) $(SRA_SDK_SYSLIBS) $(ORIG_LIBS) $(LIBXML_LIBS) $(LIBXSLT_LIBS)

POST_LINK = $(VDB_POST_LINK)

REQUIRES = objects $(VDB_REQ) LIBXML LIBXSLT


WATCHERS = bollin
