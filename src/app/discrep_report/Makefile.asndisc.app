# $Id$
#
# Makefile:  for C++ discrepancy report
# Author: J. Chen
#

###  BASIC PROJECT SETTINGS
APP = asndisc_cpp
SRC = cAsndisc

LIB = xdiscrepancy_report xvalidate valerr \
      xmlwrapp xobjedit $(OBJREAD_LIBS) $(XFORMAT_LIBS) xalnmgr xobjutil \
      tables macro xregexp $(PCRE_LIB) $(ncbi_xreader_pubseqos2) \
      ncbi_xdbapi_ftds dbapi $(OBJMGR_LIBS) $(FTDS_LIB)

LIBS = $(LIBXSLT_LIBS) $(LIBXML_LIBS) $(PCRE_LIBS) \
       $(FTDS_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CPPFLAGS= $(LIBXML_INCLUDE) $(LIBXSLT_INCLUDE) $(ORIG_CPPFLAGS) 

REQUIRES = objects

WATCHERS = chenj bollin
