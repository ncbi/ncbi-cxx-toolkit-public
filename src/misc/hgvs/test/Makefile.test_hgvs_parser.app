# $Id$

APP = test_hgvs_parser
SRC = test_hgvs_parser_app

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE) -DBOOST_ERROR_CODE_HEADER_ONLY \
           -DBOOST_SYSTEM_NO_DEPRECATED
CXXFLAGS = $(ORIG_CXXFLAGS)
LDFLAGS  = $(ORIG_LDFLAGS)

LIB_ = hgvs $(OBJREAD_LIBS) test_boost xregexp $(PCRE_LIB) xobjutil \
       entrez2cli entrez2 $(OBJMGR_LIBS)

LIB = $(LIB_:%=%$(STATIC))

LIBS = $(PCRE_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Spirit Boost.Test.Included

CHECK_COPY = test_variations.asn
CHECK_CMD  = test_hgvs_parser -in test_variations.asn /CHECK_NAME=test_hgvs_parser

WATCHERS = astashya
