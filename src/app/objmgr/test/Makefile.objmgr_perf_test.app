#################################
# $Id$
# Author:  Aleksey Grichenko (grichenk@ncbi.nlm.nih.gov)
#################################

# Build object manager performance test "objmgr_perf_test"
#################################

REQUIRES = MT objects

APP = objmgr_perf_test
SRC = objmgr_perf_test
LIB = $(ncbi_xreader_pubseqos2) \
      ncbi_xdbapi_ftds $(FTDS_LIB) xobjutil $(OBJMGR_LIBS)

LIBS = $(GENBANK_THIRD_PARTY_LIBS) $(FTDS_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)


CHECK_COPY = perf_ids1_acc perf_ids1_gi perf_ids2_acc perf_ids2_gi perf_ids3_acc perf_ids3_gi
CHECK_TIMEOUT = 300
CHECK_REQUIRES = -MSWin

CHECK_CMD = objmgr_perf_test -ids perf_ids1_acc -threads 0 -loader gb
CHECK_CMD = objmgr_perf_test -ids perf_ids1_acc -threads 0 -loader gb -pubseqos
CHECK_CMD = objmgr_perf_test -ids perf_ids1_acc -threads 0 -loader gb -no_split
CHECK_CMD = objmgr_perf_test -ids perf_ids1_acc -threads 0 -loader gb -pubseqos -no_split
CHECK_CMD = objmgr_perf_test -ids perf_ids1_acc -threads 0 -loader psg
CHECK_CMD = objmgr_perf_test -ids perf_ids1_acc -threads 0 -loader psg -no_split
CHECK_CMD = objmgr_perf_test -ids perf_ids1_acc -threads 20 -loader gb
CHECK_CMD = objmgr_perf_test -ids perf_ids1_acc -threads 20 -loader gb -pubseqos
CHECK_CMD = objmgr_perf_test -ids perf_ids1_acc -threads 20 -loader gb -no_split
CHECK_CMD = objmgr_perf_test -ids perf_ids1_acc -threads 20 -loader gb -pubseqos -no_split
CHECK_CMD = objmgr_perf_test -ids perf_ids1_acc -threads 20 -loader psg
CHECK_CMD = objmgr_perf_test -ids perf_ids1_acc -threads 20 -loader psg -no_split

CHECK_CMD = objmgr_perf_test -ids perf_ids1_gi -threads 0 -loader gb
CHECK_CMD = objmgr_perf_test -ids perf_ids1_gi -threads 0 -loader gb -pubseqos
CHECK_CMD = objmgr_perf_test -ids perf_ids1_gi -threads 0 -loader gb -no_split
CHECK_CMD = objmgr_perf_test -ids perf_ids1_gi -threads 0 -loader gb -pubseqos -no_split
CHECK_CMD = objmgr_perf_test -ids perf_ids1_gi -threads 0 -loader psg
CHECK_CMD = objmgr_perf_test -ids perf_ids1_gi -threads 0 -loader psg -no_split
CHECK_CMD = objmgr_perf_test -ids perf_ids1_gi -threads 20 -loader gb
CHECK_CMD = objmgr_perf_test -ids perf_ids1_gi -threads 20 -loader gb -pubseqos
CHECK_CMD = objmgr_perf_test -ids perf_ids1_gi -threads 20 -loader gb -no_split
CHECK_CMD = objmgr_perf_test -ids perf_ids1_gi -threads 20 -loader gb -pubseqos -no_split
CHECK_CMD = objmgr_perf_test -ids perf_ids1_gi -threads 20 -loader psg
CHECK_CMD = objmgr_perf_test -ids perf_ids1_gi -threads 20 -loader psg -no_split

CHECK_CMD = objmgr_perf_test -ids perf_ids2_acc -threads 0 -loader gb
CHECK_CMD = objmgr_perf_test -ids perf_ids2_acc -threads 0 -loader gb -pubseqos
CHECK_CMD = objmgr_perf_test -ids perf_ids2_acc -threads 0 -loader gb -no_split
CHECK_CMD = objmgr_perf_test -ids perf_ids2_acc -threads 0 -loader gb -pubseqos -no_split
CHECK_CMD = objmgr_perf_test -ids perf_ids2_acc -threads 0 -loader psg
CHECK_CMD = objmgr_perf_test -ids perf_ids2_acc -threads 0 -loader psg -no_split
CHECK_CMD = objmgr_perf_test -ids perf_ids2_acc -threads 20 -loader gb
CHECK_CMD = objmgr_perf_test -ids perf_ids2_acc -threads 20 -loader gb -pubseqos
CHECK_CMD = objmgr_perf_test -ids perf_ids2_acc -threads 20 -loader gb -no_split
CHECK_CMD = objmgr_perf_test -ids perf_ids2_acc -threads 20 -loader gb -pubseqos -no_split
CHECK_CMD = objmgr_perf_test -ids perf_ids2_acc -threads 20 -loader psg
CHECK_CMD = objmgr_perf_test -ids perf_ids2_acc -threads 20 -loader psg -no_split

CHECK_CMD = objmgr_perf_test -ids perf_ids2_gi -threads 0 -loader gb
CHECK_CMD = objmgr_perf_test -ids perf_ids2_gi -threads 0 -loader gb -pubseqos
CHECK_CMD = objmgr_perf_test -ids perf_ids2_gi -threads 0 -loader gb -no_split
CHECK_CMD = objmgr_perf_test -ids perf_ids2_gi -threads 0 -loader gb -pubseqos -no_split
CHECK_CMD = objmgr_perf_test -ids perf_ids2_gi -threads 0 -loader psg
CHECK_CMD = objmgr_perf_test -ids perf_ids2_gi -threads 0 -loader psg -no_split
CHECK_CMD = objmgr_perf_test -ids perf_ids2_gi -threads 20 -loader gb
CHECK_CMD = objmgr_perf_test -ids perf_ids2_gi -threads 20 -loader gb -pubseqos
CHECK_CMD = objmgr_perf_test -ids perf_ids2_gi -threads 20 -loader gb -no_split
CHECK_CMD = objmgr_perf_test -ids perf_ids2_gi -threads 20 -loader gb -pubseqos -no_split
CHECK_CMD = objmgr_perf_test -ids perf_ids2_gi -threads 20 -loader psg
CHECK_CMD = objmgr_perf_test -ids perf_ids2_gi -threads 20 -loader psg -no_split

CHECK_CMD = objmgr_perf_test -ids perf_ids3_acc -threads 0 -loader gb
CHECK_CMD = objmgr_perf_test -ids perf_ids3_acc -threads 0 -loader gb -pubseqos
CHECK_CMD = objmgr_perf_test -ids perf_ids3_acc -threads 0 -loader gb -no_split
CHECK_CMD = objmgr_perf_test -ids perf_ids3_acc -threads 0 -loader gb -pubseqos -no_split
CHECK_CMD = objmgr_perf_test -ids perf_ids3_acc -threads 0 -loader psg
CHECK_CMD = objmgr_perf_test -ids perf_ids3_acc -threads 0 -loader psg -no_split
CHECK_CMD = objmgr_perf_test -ids perf_ids3_acc -threads 20 -loader gb
CHECK_CMD = objmgr_perf_test -ids perf_ids3_acc -threads 20 -loader gb -pubseqos
CHECK_CMD = objmgr_perf_test -ids perf_ids3_acc -threads 20 -loader gb -no_split
CHECK_CMD = objmgr_perf_test -ids perf_ids3_acc -threads 20 -loader gb -pubseqos -no_split
CHECK_CMD = objmgr_perf_test -ids perf_ids3_acc -threads 20 -loader psg
CHECK_CMD = objmgr_perf_test -ids perf_ids3_acc -threads 20 -loader psg -no_split

CHECK_CMD = objmgr_perf_test -ids perf_ids3_gi -threads 0 -loader gb
CHECK_CMD = objmgr_perf_test -ids perf_ids3_gi -threads 0 -loader gb -pubseqos
CHECK_CMD = objmgr_perf_test -ids perf_ids3_gi -threads 0 -loader gb -no_split
CHECK_CMD = objmgr_perf_test -ids perf_ids3_gi -threads 0 -loader gb -pubseqos -no_split
CHECK_CMD = objmgr_perf_test -ids perf_ids3_gi -threads 0 -loader psg
CHECK_CMD = objmgr_perf_test -ids perf_ids3_gi -threads 0 -loader psg -no_split
CHECK_CMD = objmgr_perf_test -ids perf_ids3_gi -threads 20 -loader gb
CHECK_CMD = objmgr_perf_test -ids perf_ids3_gi -threads 20 -loader gb -pubseqos
CHECK_CMD = objmgr_perf_test -ids perf_ids3_gi -threads 20 -loader gb -no_split
CHECK_CMD = objmgr_perf_test -ids perf_ids3_gi -threads 20 -loader gb -pubseqos -no_split
CHECK_CMD = objmgr_perf_test -ids perf_ids3_gi -threads 20 -loader psg
CHECK_CMD = objmgr_perf_test -ids perf_ids3_gi -threads 20 -loader psg -no_split

CHECK_CMD = objmgr_perf_test -ids perf_ids3_acc -loader gb -bulk data
CHECK_CMD = objmgr_perf_test -ids perf_ids3_acc -loader gb -pubseqos -bulk data
CHECK_CMD = objmgr_perf_test -ids perf_ids3_acc -loader gb -no_split -bulk data
CHECK_CMD = objmgr_perf_test -ids perf_ids3_acc -loader gb -pubseqos -no_split -bulk data
CHECK_CMD = objmgr_perf_test -ids perf_ids3_acc -loader psg -bulk data
CHECK_CMD = objmgr_perf_test -ids perf_ids3_acc -loader psg -no_split -bulk data

CHECK_CMD = objmgr_perf_test -ids perf_ids1_gi -loader gb -bulk bioseq
CHECK_CMD = objmgr_perf_test -ids perf_ids1_gi -loader gb -pubseqos -bulk bioseq
CHECK_CMD = objmgr_perf_test -ids perf_ids1_gi -loader gb -no_split -bulk bioseq
CHECK_CMD = objmgr_perf_test -ids perf_ids1_gi -loader gb -pubseqos -no_split -bulk bioseq
CHECK_CMD = objmgr_perf_test -ids perf_ids1_gi -loader psg -bulk bioseq
CHECK_CMD = objmgr_perf_test -ids perf_ids1_gi -loader psg -no_split -bulk bioseq

WATCHERS = grichenk
