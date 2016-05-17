# $Id$
#
# Makefile:  for C++ discrepancy report
# Author: Sema
#

###  BASIC PROJECT SETTINGS
APP = asndisc
SRC = asndisc

LIB = xdiscrepancy xvalidate valerr \
      xmlwrapp xobjedit $(OBJREAD_LIBS) $(XFORMAT_LIBS) xalnmgr xobjutil \
      tables macro xregexp $(PCRE_LIB) $(ncbi_xreader_pubseqos2) \
      ncbi_xdbapi_ftds dbapi $(OBJMGR_LIBS) $(FTDS_LIB)

LIBS = $(LIBXSLT_LIBS) $(LIBXML_LIBS) $(PCRE_LIBS) \
       $(FTDS_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CPPFLAGS= $(LIBXML_INCLUDE) $(LIBXSLT_INCLUDE) $(ORIG_CPPFLAGS) -I$(import_root)/../include

LDFLAGS = -L$(import_root)/../lib $(ORIG_LDFLAGS)

REQUIRES = objects

WATCHERS = kachalos
