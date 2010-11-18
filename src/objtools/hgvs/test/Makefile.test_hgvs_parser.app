#

###  BASIC PROJECT SETTINGS
APP = test_hgvs_parser
SRC = test_hgvs_parser_app
# OBJ =

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB_ = test_boost $(SEQ_LIBS) genome_collection pub medline biblio \
    ncbi_xdbapi_ftds dbapi dbapi_driver \
    xalgoalignsplign xalgoalignnw xalgoalignutil xalgoseq xalnmgr taxon1 $(BLAST_LIBS) seqmasks_io xconnect xqueryparse xregexp $(PCRE_LIB) \
    $(OBJMGR_LIBS) general xser xutil xncbi
    
#LIB_ALNDB = \
#       genomealignsdb_dataloader genomealignsdb_query \
#       alndb_asnclient alndb_query  \
#       generic_db_core dbassist dbloadb tea_crypt asnmuxdblb mux \
#       dbloadb_asn asngendefs generic_db_asn generic_db_utils \
#       ncbi_xdbapi_ftds dbapi dbapi_driver \
#       xalnmgr xobjmgr xobjutil xobjread creaders \
#       taxon1 scoremat tables $(OBJMGR_LIBS)

LIB = hgvs $(LIB_:%=%$(STATIC)) $(FTDS_LIB) $(LIB_ALNDB:%=%$(STATIC))
    
LIBS = $(PCRE_LIBS) $(FTDS_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

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
