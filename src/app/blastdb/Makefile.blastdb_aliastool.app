APP = blastdb_aliastool
SRC = blastdb_aliastool
LIB_ = writedb seqdb xblast xnetblast xnetblastcli composition_adjustment \
	xalgoblastdbindex xalgowinmask scoremat blastdb tables xobjread \
	$(OBJMGR_LIBS)
LIB = $(LIB_:%=%$(STATIC))

CFLAGS   = $(FAST_CFLAGS)
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

CPPFLAGS = $(ORIG_CPPFLAGS)
LIBS = $(CMPRS_LIBS) $(DL_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

REQUIRES = objects -Cygwin
