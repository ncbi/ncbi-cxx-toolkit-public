# $Id$

APP = convert_seq
SRC = convert_seq
LIB = $(XFORMAT_LIBS) xalnmgr gbseq xobjutil $(OBJREAD_LIBS) tables submit \
      xregexp $(PCRE_LIB) $(OBJMGR_LIBS) xobjwrite ncbi_xloader_wgs

LIBS = $(PCRE_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects -Cygwin -BSD

WATCHERS = ucko drozdov
