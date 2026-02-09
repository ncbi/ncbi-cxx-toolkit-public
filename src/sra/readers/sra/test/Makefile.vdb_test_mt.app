#################################
# $Id$
# Author:  Eugene Vasilchenko
#################################

# Build application "vdb_test_mt"
#################################

APP = vdb_test_mt
SRC = vdb_test_mt

LIB =   $(SRAREAD_LIBS) $(SOBJMGR_LDEP) $(CMPRS_LIB)
LIBS =  $(SRA_SDK_SYSLIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)
POST_LINK = $(VDB_POST_LINK)

REQUIRES = objects

CPPFLAGS = $(ORIG_CPPFLAGS) $(SRA_INCLUDE)

# old SRA
CHECK_CMD = vdb_test_mt -acc SRR035417 -refseq_table -quality_graph -seq_entry  -scan_reads /CHECK_NAME=vdb_test_mt_SRA
# new cSRA w/o REFERENCE
CHECK_CMD = vdb_test_mt -acc SRR749060 -refseq_table -quality_graph -seq_entry  -scan_reads /CHECK_NAME=vdb_test_mt_cSRA_no_REF
# new cSRA
CHECK_CMD = vdb_test_mt -acc SRR413273 -refseq_table -q NM_004119.2:0-10000 -ref_seq -stat_graph -quality_graph -seq_entry -scan_reads -repeats 10 /CHECK_NAME=vdb_test_mt_cSRA
CHECK_CMD = vdb_test_mt -acc SRR413273 -refseq_table -q NM_004119.2:0-10000 -ref_seq -stat_graph -quality_graph -seq_entry -scan_reads -repeats 10 -use_vdb /CHECK_NAME=vdb_test_mt_cSRA_VDB

CHECK_TIMEOUT = 600

WATCHERS = vasilche ucko grichenk
