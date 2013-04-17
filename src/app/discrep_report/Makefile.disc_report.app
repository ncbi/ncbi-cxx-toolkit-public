# $Id$
#
# Makefile:  for C++ discrepancy report
# Author: J. Chen
#

###  BASIC PROJECT SETTINGS
APP = disc_report
SRC = cDiscRep_app cchecking_class cauto_disc_class cDiscRep_config \
	cDiscRep_tests cDiscRep_output cDiscRep_util cUtilib cDiscRep_susrule

LIB = xvalidate valid valerr taxon3 $(XFORMAT_LIBS) xalnmgr xobjutil xconnect tables \
	taxon1 submit macro xregexp $(PCRE_LIB) $(OBJMGR_LIBS)

LIBS = $(PCRE_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)
 
REQUIRES = objects
