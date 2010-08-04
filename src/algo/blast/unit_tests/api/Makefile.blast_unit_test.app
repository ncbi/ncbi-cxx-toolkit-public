# $Id$

APP = blast_unit_test
# N.B.: if you remove sources, don't remove blast_unit_test lest you #undef
# NCBI_BOOST_NO_AUTO_TEST_MAIN in another source file
SRC = test_objmgr blast_test_util blast_unit_test bl2seq_unit_test \
    gencode_singleton_unit_test blastoptions_unit_test blastfilter_unit_test \
    uniform_search_unit_test remote_blast_unit_test aascan_unit_test \
    ntscan_unit_test version_reference_unit_test aalookup_unit_test \
    subj_ranges_unit_test blastengine_unit_test linkhsp_unit_test \
    blasthits_unit_test gapinfo_unit_test rps_unit_test hspstream_unit_test \
    hspstream_test_util scoreblk_unit_test seqalign_cmp seqalign_set_convert \
    split_query_unit_test phiblast_unit_test prelimsearch_unit_test \
	psiblast_unit_test psibl2seq_unit_test traceback_unit_test \
	tracebacksearch_unit_test msa2pssm_unit_test optionshandle_unit_test \
	hspfilter_culling_unit_test hspfilter_besthit_unit_test \
    psiblast_iteration_unit_test pssmcreate_unit_test blastdiag_unit_test \
    blastextend_unit_test blastsetup_unit_test pssmenginefreqratios_unit_test \
    querydata_unit_test queryinfo_unit_test redoalignment_unit_test \
    search_strategy_unit_test setupfactory_unit_test mockseqsrc1_unit_test \
	mockseqsrc2_unit_test seqinfosrc_unit_test ntlookup_unit_test \
	seqsrc_unit_test seqsrc_mock

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE) -I$(srcdir)/../../api \
           -I$(srcdir)/../../core -I$(top_srcdir)/algo/blast/api \
           -I$(top_srcdir)/algo/blast/core

LIB = test_boost $(BLAST_INPUT_LIBS) ncbi_xloader_blastdb_rmt \
    $(BLAST_LIBS) xobjsimple $(OBJMGR_LIBS:ncbi_x%=ncbi_x%$(DLL))

LIBS = $(NETWORK_LIBS) \
        $(PCRE_LIBS) $(CMPRS_LIBS) $(DL_LIBS) $(ORIG_LIBS)
LDFLAGS = $(FAST_LDFLAGS)

CHECK_REQUIRES = MT in-house-resources
CHECK_CMD = blast_unit_test
CHECK_COPY = blast_unit_test.ini data
# This unit test suite shouldn't run longer than 15 minutes
CHECK_TIMEOUT = 900

WATCHERS = blastsoft
