#################################
# $Id$
# Author:  Frank Ludwig
#################################

# Build application "streamtest"
#################################

APP = streamtest
SRC = streamtest
LIB = prosplign xalgoalignutil xalgoseq xmlwrapp \
      xvalidate xcleanup xobjwrite variation_utils $(XFORMAT_LIBS) \
      valerr taxon1 $(BLAST_LIBS) \
      ncbi_xloader_wgs $(SRAREAD_LIBS) xqueryparse xregexp $(PCRE_LIB) $(OBJMGR_LIBS) \
      $(OBJEDIT_LIBS)

LIBS = $(SRA_SDK_SYSLIBS) $(LIBXSLT_LIBS) $(LIBXML_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) \
      $(PCRE_LIBS) $(ORIG_LIBS)

REQUIRES = objects algo LIBXSLT $(VDB_REQ)

WATCHERS = kans kornbluh drozdov
