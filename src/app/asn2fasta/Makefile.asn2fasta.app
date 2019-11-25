#################################
# $Id$
# Author:  Mati Shomrat
#################################

# Build application "asn2fasta"
#################################

APP = asn2fasta
SRC = asn2fasta
LIB = $(DATA_LOADERS_UTIL_LIB) \
      xobjwrite variation_utils $(XFORMAT_LIBS) \
      xalnmgr xobjutil valerr xregexp \
      $(OBJMGR_LIBS) xregexp $(PCRE_LIB)


LIBS = $(DATA_LOADERS_UTIL_LIBS) $(SRA_SDK_SYSLIBS) \
       $(CMPRS_LIBS) $(NETWORK_LIBS) \
       $(DL_LIBS) $(PCRE_LIBS) $(ORIG_LIBS)

POST_LINK = $(VDB_POST_LINK)

REQUIRES = objects


WATCHERS = ludwigf foleyjp
