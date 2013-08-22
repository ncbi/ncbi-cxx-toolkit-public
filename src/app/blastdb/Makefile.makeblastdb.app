WATCHERS = camacho maning 

APP = makeblastdb
SRC = makeblastdb masked_range_set
LIB_ = $(BLAST_INPUT_LIBS) writedb $(BLAST_LIBS) $(OBJMGR_LIBS)
LIB = $(LIB_:%=%$(STATIC))

CFLAGS   = $(FAST_CFLAGS) 
CXXFLAGS = $(FAST_CXXFLAGS) -fopenmp
LDFLAGS  = $(FAST_LDFLAGS)

CPPFLAGS = -DNCBI_MODULE=BLASTDB $(ORIG_CPPFLAGS)
LIBS = -fopenmp $(CMPRS_LIBS) $(DL_LIBS) $(NETWORK_LIBS) -lstdc++ $(ORIG_LIBS)

REQUIRES = objects -Cygwin
