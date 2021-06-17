# $Id$

APP = convert_seq
SRC = convert_seq gff_reader
LIB = $(ncbi_xloader_wgs) $(SRAREAD_LIBS) xobjwrite variation_utils \
      $(OBJREAD_LIBS) $(XFORMAT_LIBS) xalnmgr xobjutil tables xregexp \
      $(PCRE_LIB) $(OBJMGR_LIBS)

LIBS = $(GENBANK_THIRD_PARTY_LIBS) \
       $(SRA_SDK_SYSLIBS) $(PCRE_LIBS) $(CMPRS_LIBS) \
       $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

POST_LINK = $(VDB_POST_LINK)

REQUIRES = objects

WATCHERS = ucko drozdov foleyjp
