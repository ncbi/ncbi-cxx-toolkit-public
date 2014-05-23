# $Id$
#
# Makefile:  for C++ discrepancy report
# Author: J. Chen
#

###  BASIC PROJECT SETTINGS
APP = asndisc_cpp
SRC = cAsndisc

LIB = xdiscrepancy_report xvalidate xobjedit valid valerr \
	  xmlwrapp taxon3 $(XFORMAT_LIBS) xalnmgr xobjutil tables \
	  macro xregexp $(PCRE_LIB) $(OBJREAD_LIBS) $(ncbi_xreader_pubseqos2) \
	  $(OBJMGR_LIBS)

LIBS = $(LIBXSLT_LIBS) $(LIBXML_LIBS) $(PCRE_LIBS) \
       $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CPPFLAGS= $(LIBXML_INCLUDE) $(LIBXSLT_INCLUDE) $(ORIG_CPPFLAGS) 

REQUIRES = objects

WATCHERS = chenj bollin
