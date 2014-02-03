# $Id$
#
# Makefile:  for C++ discrepancy report
# Author: J. Chen
#

###  BASIC PROJECT SETTINGS
APP = disc_report
SRC = cApiDisc

PRE_LIBS = /home/chenj/DisRepLib/trunk/c++/lib/libxdiscrepancy_report.a

#LIB = xdiscrepancy_report xvalidate xobjedit valid valerr \

LIB = xvalidate xobjedit valid valerr \
        taxon3 $(XFORMAT_LIBS) xalnmgr xobjutil tables \
        macro xregexp $(PCRE_LIB) $(OBJREAD_LIBS) $(OBJMGR_LIBS)

LIBS = $(PCRE_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects

WATCHERS = chenj bollin
