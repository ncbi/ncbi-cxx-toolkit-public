###############################
# $Id$
###############################

APP = test_validator
SRC = test_validator
LIB = xvalidate $(XFORMAT_LIBS) xalnmgr xobjutil valerr submit tables gbseq \
      xregexp $(PCRE_LIB) $(OBJMGR_LIBS) \
      $(OBJEDIT_LIBS)

LIBS =  $(PCRE_LIBS) $(CMPRS_LIBS) $(DL_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

CHECK_CMD  = test_validator.sh
CHECK_COPY = current.prt test_validator.sh

REQUIRES = -Cygwin objects

WATCHERS = bollin kornbluh kans foleyjp asztalos

