#################################
# $Id$
# Author:  Colleen Bollin, based on file by Mati Shomrat
#################################

# Build application "asnval"
#################################

APP = asnvalidate
SRC = asnval
LIB = xvalidate xcleanup $(XFORMAT_LIBS) xalnmgr xobjutil \
      xmlwrapp ncbi_xloader_wgs $(SRAREAD_LIBS) valerr submit gbseq \
      tables xregexp $(PCRE_LIB) $(OBJMGR_LIBS) \
      $(OBJEDIT_LIBS)

#LIBS = $(PCRE_LIBS) $(CMPRS_LIBS) $(DL_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS) $(LIBXML_LIBS)
LIBS = $(VDB_LIBS) $(LIBXSLT_LIBS) $(LIBXML_LIBS) $(PCRE_LIBS) \
       $(CMPRS_LIBS) $(DL_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)


REQUIRES = objects LIBXML LIBXSLT $(VDB_REQ)

CXXFLAGS += $(ORIG_CXXFLAGS)
LDFLAGS  += $(ORIG_LDFLAGS)

WATCHERS = bollin kornbluh gotvyans
