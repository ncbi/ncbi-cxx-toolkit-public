###############################
# $Id$
###############################

APP = test_validator
SRC = test_validator
LIB = xvalidate xformat xalnmgr xobjutil valerr submit tables taxon3 gbseq \
      $(OBJMGR_LIBS)

LIBS = $(CMPRS_LIBS) $(DL_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

CHECK_CMD  = test_validator.sh
CHECK_COPY = current.prt test_validator.sh
CHECK_AUTHORS = bollin

REQUIRES = -Cygwin
