#################################
# $Id$
# Author:  Aleksey Grichenko (grichenk@ncbi.nlm.nih.gov)
#################################

# Build object manager performance test "objmgr_perf_test"
#################################

REQUIRES = objects

APP = objmgr_perf_test
SRC = objmgr_perf_test
LIB = ncbi_xreader_id1 ncbi_xreader_id2 ncbi_xreader_pubseqos ncbi_xreader_pubseqos2 \
      ncbi_xdbapi_ftds $(FTDS_LIB) xobjutil $(OBJMGR_LIBS)

LIBS = $(FTDS_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)


#CHECK_COPY = all_readers.sh
#CHECK_CMD = all_readers.sh objmgr_demo -id 568815307 -resolve all -adaptive
#CHECK_CMD = all_readers.sh objmgr_demo -id ABYI02000001 -resolve all -adaptive

WATCHERS = grichenk
