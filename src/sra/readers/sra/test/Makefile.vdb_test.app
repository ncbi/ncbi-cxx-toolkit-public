#################################
# $Id$
# Author:  Eugene Vasilchenko
#################################

# Build application "vdb_test"
#################################

APP = vdb_test
SRC = vdb_test

LIB =   $(SRAREAD_LIBS) $(SOBJMGR_LIBS) $(CMPRS_LIB)
LIBS =  $(SRA_SDK_SYSLIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

REQUIRES = objects

CPPFLAGS = $(ORIG_CPPFLAGS) $(SRA_INCLUDE)

CHECK_REQUIRES = in-house-resources -MSWin -Solaris
# default - old fixed SRA
CHECK_CMD = vdb_test
CHECK_CMD = vdb_test -refseq_table -q GL000207.1:0-10000 -ref_seq -stat_graph -quality_graph -seq_entry -scan_reads /CHECK_NAME=vdb_test_SRA_fixed
# old SRA
CHECK_CMD = vdb_test -acc SRR035417 -refseq_table -quality_graph -seq_entry  -scan_reads /CHECK_NAME=vdb_test_SRA
# new cSRA w/o REFERENCE
CHECK_CMD = vdb_test -acc SRR749060 -refseq_table -quality_graph -seq_entry  -scan_reads /CHECK_NAME=vdb_test_cSRA_no_REF
# new cSRA
CHECK_CMD = vdb_test -acc SRR413273 -refseq_table -q NM_004119.2:0-10000 -ref_seq -stat_graph -quality_graph -seq_entry -scan_reads /CHECK_NAME=vdb_test_cSRA
CHECK_CMD = vdb_test -acc SRR000000 -no_acc /CHECK_NAME=vdb_test_none

WATCHERS = vasilche ucko
