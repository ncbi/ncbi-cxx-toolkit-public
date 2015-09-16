#################################
# $Id$
# Author:  Colleen Bollin, based on file by Mati Shomrat
#################################

# Build application "asn_cleanup"
#################################

APP = asn_cleanup
SRC = asn_cleanup
LIB = $(OBJEDIT_LIBS) $(XFORMAT_LIBS) xobjutil xalnmgr xcleanup valid valerr xregexp \
	  entrez2cli entrez2 tables \
	  ncbi_xdbapi_ftds dbapi dbapi_driver $(FTDS_LIB) \
	  $(ncbi_xreader_pubseqos2) \
	  $(OBJMGR_LIBS) $(PCRE_LIB)

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(PCRE_LIBS) $(FTDS_LIBS) \
	   $(ORIG_LIBS)

REQUIRES = objects -Cygwin

CXXFLAGS += $(ORIG_CXXFLAGS)
LDFLAGS  += $(ORIG_LDFLAGS)


WATCHERS = bollin kornbluh
