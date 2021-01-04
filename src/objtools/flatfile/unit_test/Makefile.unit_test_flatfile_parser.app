# $Id$

APP = unit_test_flatfile_parser
SRC = unit_test_flatfile_parser

CPPFLAGS = $(LIBXML_INCLUDE) $(LIBXSLT_INCLUDE) $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB  = xflatfile test_boost fix_pub eutils_client xmlwrapp $(XFORMAT_LIBS) xalnmgr xobjutil taxon1 $(SEQ_LIBS) pub medline biblio general xser xconnect xutil xncbi tables xregexp $(PCRE_LIB) $(OBJMGR_LIBS)
LIBS = $(CMPRS_LIBS) $(PCRE_LIBS) $(GENBANK_THIRD_PARTY_LIBS) $(LIBXSLT_LIBS) $(LIBXML_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test.Included

WATCHERS = foleyjp
