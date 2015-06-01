# $Id$

APP = convert_seq
SRC = convert_seq
LIB = ncbi_xloader_wgs $(SRAREAD_LIBS) $(XFORMAT_LIBS) xalnmgr xobjwrite \
      $(OBJREAD_LIBS) xobjutil tables xregexp $(PCRE_LIB) $(OBJMGR_LIBS)

LIBS = $(SRA_SDK_SYSLIBS) $(PCRE_LIBS) $(CMPRS_LIBS) \
       $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects $(VDB_REQ)

WATCHERS = ucko drozdov
