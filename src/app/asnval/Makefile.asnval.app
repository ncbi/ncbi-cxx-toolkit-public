#################################
# $Id$
# Author:  Colleen Bollin, based on file by Mati Shomrat
#################################

# Build application "asnval"
#################################

APP = asnvalidate
SRC = asnval
LIB = xvalidate xcleanup $(XFORMAT_LIBS) xalnmgr xobjutil \
      valid valerr submit taxon3 gbseq xmlwrapp\
      tables xregexp $(PCRE_LIB) $(OBJMGR_LIBS)

LIBS = $(PCRE_LIBS) $(CMPRS_LIBS) $(DL_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS) $(LIBXML_LIBS)

REQUIRES = objects LIBXML LIBXSLT -Cygwin


WATCHERS = bollin kornbluh gotvyans
