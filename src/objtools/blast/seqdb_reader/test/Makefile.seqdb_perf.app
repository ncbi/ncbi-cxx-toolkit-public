# $Id$

APP = seqdb_perf
SRC = seqdb_perf
LIB_ = seqdb xobjutil blastdb $(SOBJMGR_LIBS)
LIB = $(LIB_:%=%$(STATIC))

CFLAGS    = $(FAST_CFLAGS) $(OPENMP_FLAGS)
CXXFLAGS  = $(FAST_CXXFLAGS) $(OPENMP_FLAGS)
LDFLAGS   = $(FAST_LDFLAGS) $(OPENMP_FLAGS)

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

WATCHERS = maning madden camacho

CHECK_REQUIRES = in-house-resources
CHECK_CMD = seqdb_perf -db pataa -dbtype prot -scan_uncompressed -num_threads 4 /CHECK_NAME = scan_blastdb_mt
CHECK_CMD = seqdb_perf -db pataa -dbtype prot -scan_uncompressed -num_threads 1 /CHECK_NAME = scan_blastdb_st
CHECK_CMD = seqdb_perf -db pataa -dbtype prot -get_metadata /CHECK_NAME = get_blastdb_metadata

# This unit test suite shouldn't run longer than 15 minutes
CHECK_TIMEOUT = 900
