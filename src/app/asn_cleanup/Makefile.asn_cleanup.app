#################################
# $Id$
# Author:  Colleen Bollin, based on file by Mati Shomrat
#################################

# Build application "asn_cleanup"
#################################

APP = asn_cleanup
SRC = asn_cleanup read_hooks bigfile_processing
LIB =  xvalidate $(OBJEDIT_LIBS) $(XFORMAT_LIBS) xalnmgr xobjutil \
       valerr taxon1 tables xregexp \
      data_loaders_util ncbi_xdbapi_ftds dbapi $(FTDS_LIB) \
      $(SRAREAD_LIBS) $(OBJMGR_LIBS) $(PCRE_LIB)

LIBS = $(GENBANK_THIRD_PARTY_LIBS) \
       $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(PCRE_LIBS) $(FTDS_LIBS) \
       $(SRA_SDK_SYSLIBS) \
       $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)


POST_LINK = $(VDB_POST_LINK)

REQUIRES = objects -Cygwin $(VDB_REQ)

CXXFLAGS += $(ORIG_CXXFLAGS)
LDFLAGS  += $(ORIG_LDFLAGS)


WATCHERS = bollin
