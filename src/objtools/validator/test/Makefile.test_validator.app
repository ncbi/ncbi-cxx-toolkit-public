###############################
# $Id$
###############################

APP = test_validator
SRC = test_validator
LIB = xvalidate xobjedit $(XFORMAT_LIBS) xalnmgr xobjutil valerr submit tables taxon3 gbseq \
      valid xregexp $(PCRE_LIB) $(OBJMGR_LIBS)

LIBS =  $(PCRE_LIBS) $(CMPRS_LIBS) $(DL_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

CHECK_CMD  = test_validator.sh
CHECK_COPY = current.prt test_validator.sh

REQUIRES = -Cygwin objects

WATCHERS = bollin kornbluh

