# $Id$
#
# Makefile:  for C++ discrepancy report
# Author: J. Chen
#

###  BASIC PROJECT SETTINGS
APP = disc_report
SRC = cDiscRep_app cchecking_class cauto_disc_class cDiscRep_config \
	cDiscRep_tests cDiscRep_output cDiscRep_util cUtilib

LIB = xobjutil submit macro xobjutil $(OBJMGR_LIBS)

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)
 
REQUIRES = objects
