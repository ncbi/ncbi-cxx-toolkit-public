# $Id$

APP = blastsrainput_unit_test
SRC = blastsrainput_unit_test 

DLL_LIB = xblast align_format ncbi_xloader_blastdb_rmt ncbi_xloader_blastdb seqdb $(SRAREAD_LIBS) $(OBJREAD_LIBS) $(OBJMGR_LIBS)

CPPFLAGS = -DNCBI_MODULE=BLAST $(VDB_INCLUDE) $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)
LIB = test_boost blast_sra_input $(BLAST_INPUT_LIBS) \
    $(BLAST_LIBS) $(SRAREAD_LIBS) $(OBJMGR_LIBS:ncbi_x%=ncbi_x%$(DLL))

LIBS = $(VDB_LIBS) $(NETWORK_LIBS) $(CMPRS_LIBS) $(ORIG_LIBS)

CHECK_REQUIRES = MT in-house-resources
CHECK_CMD = magicblast_unit_test
CHECK_COPY = data magicblast_unit_test.ini

REQUIRES = VDB

WATCHERS = boratyng
