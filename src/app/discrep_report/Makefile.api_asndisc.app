# $Id$
#
# Makefile:  for C++ discrepancy report
# Author: J. Chen
#

###  BASIC PROJECT SETTINGS
APP = api_asndisc
SRC = cApiAsndisc

LIB = xdiscrepancy_report xobjread xvalidate valid valerr taxon3 \
        $(XFORMAT_LIBS) xalnmgr xobjutil xconnect tables \
        taxon1 submit macro xregexp $(PCRE_LIB) $(OBJMGR_LIBS)

LIBS = $(PCRE_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects

WATCHERS = chenj
