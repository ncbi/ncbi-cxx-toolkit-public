#

###  BASIC PROJECT SETTINGS
APP = test_hgvs_parser
SRC = test_hgvs_parser_app
# OBJ =

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB_ = hgvs variation seqfeat test_boost xregexp $(PCRE_LIB) xobjutil $(OBJMGR_LIBS)

LIB = $(LIB_:%=%$(STATIC))
    
LIBS = $(PCRE_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)
    
REQUIRES = Boost.Spirit Boost.Test.Included
CHECK_CMD = ./test_hgvs_parser_app -asn -in test_variation_feats.asn
WATCHERS = astashya

## If you need the C toolkit...
# LIBS     = $(NCBI_C_LIBPATH) -lncbi $(NETWORK_LIBS) $(ORIG_LIBS)
# CPPFLAGS = $(ORIG_CPPFLAGS) $(NCBI_C_INCLUDE)

###  EXAMPLES OF OTHER SETTINGS THAT MIGHT BE OF INTEREST
# PRE_LIBS = $(NCBI_C_LIBPATH) .....
# CFLAGS   = $(FAST_CFLAGS)
# CXXFLAGS = $(FAST_CXXFLAGS)
# LDFLAGS  = $(FAST_LDFLAGS)
