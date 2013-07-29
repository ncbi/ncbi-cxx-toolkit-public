APP = blast_services_unit_test
SRC = blast_services_test

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)
CFLAGS   = $(FAST_CFLAGS)
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

LIB = test_boost blast_services xnetblastcli xnetblast seqdb blastdb scoremat \
      xconnect $(SOBJMGR_LIBS)

LIBS = $(NETWORK_LIBS) $(CMPRS_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CHECK_CMD = blast_services_unit_test

WATCHERS = madden camacho
