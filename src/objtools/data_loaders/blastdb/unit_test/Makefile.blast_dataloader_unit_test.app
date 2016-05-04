APP = blast_dataloader_unit_test
SRC = local_dataloader_test rmt_dataloader_test

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)
CFLAGS   = $(FAST_CFLAGS)
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

LIB = test_boost ncbi_xloader_blastdb_rmt ncbi_xloader_blastdb blast_services \
      xnetblastcli xnetblast seqdb blastdb scoremat $(OBJMGR_LIBS)

LIBS = $(NETWORK_LIBS) $(CMPRS_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CHECK_REQUIRES = in-house-resources
CHECK_CMD = blast_dataloader_unit_test

WATCHERS = madden camacho
