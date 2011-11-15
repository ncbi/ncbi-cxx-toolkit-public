# $Id$

APP = align_format_unit_test
SRC = showdefline_unit_test showalign_unit_test blast_test_util \
vectorscreen_unit_test tabularinof_unit_test aln_printer_unit_test

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS = $(FAST_LDFLAGS)

LIB_ = test_boost $(BLAST_DB_DATA_LOADER_LIBS) align_format taxon1 blastdb_format \
     	gene_info xalnmgr xcgi xhtml seqmasks_io seqdb blast_services xobjutil \
        xobjread creaders xnetblastcli xnetblast blastdb scoremat tables xalnmgr $(OBJMGR_LIBS)

LIB = $(LIB_:%=%$(STATIC))
LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CHECK_CMD = align_format_unit_test
CHECK_COPY = data
CHECK_REQUIRES = -in-house-resources

REQUIRES = Boost.Test.Included

WATCHERS = zaretska jianye madden camacho
CHECK_TIMEOUT = 900
