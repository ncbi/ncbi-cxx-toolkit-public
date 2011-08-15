# $Id$

APP = test_hgvs_parser
SRC = test_hgvs_parser_app

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB_ = hgvs variation test_boost xregexp $(PCRE_LIB) xobjutil \
       entrez2cli entrez2 $(OBJMGR_LIBS)

LIB = $(LIB_:%=%$(STATIC))

LIBS = $(PCRE_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Spirit Boost.Test.Included

# Check is temporarily disabled (TODO)
# CHECK_COPY = test_variation_feats.asn
# CHECK_CMD  = ./test_hgvs_parser -in test_variation_feats.asn

WATCHERS = astashya
