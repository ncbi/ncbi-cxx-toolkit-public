#################################
# $Id$
# Author:  Mati Shomrat
#################################

# Build application "asn2flat"
#################################

APP = asn2flat
SRC = asn2flat

LIB  = xcleanup valid valerr $(OBJREAD_LIBS) $(XFORMAT_LIBS) \
	   xalnmgr xobjutil entrez2cli entrez2 tables \
	   ncbi_xdbapi_ftds dbapi dbapi_driver $(FTDS_LIB) \
	   xregexp $(PCRE_LIB) $(ncbi_xreader_pubseqos2) $(OBJMGR_LIBS)
LIBS = $(FTDS_LIBS) $(CMPRS_LIBS) $(PCRE_LIBS) $(NETWORK_LIBS) \
	   $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects -Cygwin

WATCHERS = ludwigf kornbluh
