#################################
# $Id$
# Author:  Colleen Bollin, based on file by Mati Shomrat
#################################

# Build application "table2asn"
#################################

APP = table2asn
SRC = table2asn multireader
LIB = xvalidate xcleanup xalnmgr xobjutil \
      valid valerr submit taxon3 gbseq \
	  xalgophytree biotree fastme xalnmgr tables xobjreadex xobjread \
      tables xregexp $(XFORMAT_LIBS) $(PCRE_LIB) $(OBJMGR_LIBS)

LIBS = $(PCRE_LIBS) $(CMPRS_LIBS) $(DL_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

REQUIRES = objects -Cygwin


WATCHERS = bollin gotvyans

