# $Id$
#
# Makefile:  for C++ discrepancy report
# Author: Sema
#

###  BASIC PROJECT SETTINGS
APP = asndisc
SRC = asndisc

LIB = xdiscrepancy xvalidate valerr \
      xmlwrapp $(XFORMAT_LIBS) xalnmgr xobjutil tables \
      macro xregexp $(PCRE_LIB) $(OBJREAD_LIBS) $(ncbi_xreader_pubseqos2) \
      ncbi_xdbapi_ftds dbapi dbapi_driver $(FTDS_LIB) $(OBJMGR_LIBS) \
      $(OBJEDIT_LIBS)

LIBS = $(LIBXSLT_LIBS) $(LIBXML_LIBS) $(PCRE_LIBS) \
       $(FTDS_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CPPFLAGS= $(LIBXML_INCLUDE) $(LIBXSLT_INCLUDE) $(ORIG_CPPFLAGS) 

REQUIRES = objects

WATCHERS = kachalos
