# $Id$

APP = seqdb_perf
SRC = seqdb_perf
LIB_ = seqdb xobjutil blastdb $(SOBJMGR_LIBS)
LIB = $(LIB_:%=%$(STATIC))

CFLAGS    = $(FAST_CFLAGS) -fopenmp
CXXFLAGS  = $(FAST_CXXFLAGS) -fopenmp
LOCAL_LDFLAGS = -fopenmp
LDFLAGS   = $(FAST_LDFLAGS)

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

WATCHERS = maning madden camacho

CHECK_REQUIRES = in-house-resources
CHECK_CMD = seqdb_perf -db pataa -dbtype prot -scan_uncompressed -num_threads 4 /CHECK_NAME = scan_blastdb_mt
CHECK_CMD = seqdb_perf -db pataa -dbtype prot -scan_uncompressed -num_threads 1 /CHECK_NAME = scan_blastdb_st
CHECK_CMD = seqdb_perf -db pataa -dbtype prot -get_metadata /CHECK_NAME = get_blastdb_metadata

