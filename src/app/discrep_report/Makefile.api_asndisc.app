# $Id$
#
# Makefile:  for C++ discrepancy report
# Author: J. Chen
#

###  BASIC PROJECT SETTINGS
APP = api_asndisc
SRC = cApiAsndisc

PRE_LIBS = /home/chenj/DisRepLib/trunk/c++/lib/libxdiscrepancy_report.a

LIB = xobjread xvalidate valid valerr taxon3 \
        $(XFORMAT_LIBS) xalnmgr xobjutil xconnect tables \
	taxon1 submit macro xregexp $(PCRE_LIB) $(OBJMGR_LIBS)

LIBS = $(PCRE_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects

WATCHERS = chenj
