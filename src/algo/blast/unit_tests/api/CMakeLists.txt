#############################################################################
# $Id$
#############################################################################

NCBI_requires(Boost.Test.Included)
NCBI_add_include_directories(${NCBI_CURRENT_SOURCE_DIR}/../../api)
NCBI_add_definitions(NCBI_MODULE=BLAST)

NCBI_add_library(blast_unit_test_util seqalign_util)

NCBI_add_app(
blast_unit_test
aalookup_unit_test
seqsrc_unit_test
delta_unit_test
ntlookup_unit_test
seqinfosrc_unit_test
setupfactory_unit_test
search_strategy_unit_test
redoalignment_unit_test
queryinfo_unit_test
querydata_unit_test
pssmenginefreqratios_unit_test
blastsetup_unit_test
blastextend_unit_test
blastdiag_unit_test
pssmcreate_unit_test
psiblast_iteration_unit_test
hspfilter_besthit_unit_test
hspfilter_culling_unit_test
optionshandle_unit_test
msa2pssm_unit_test
tracebacksearch_unit_test
traceback_unit_test
psibl2seq_unit_test
psiblast_unit_test
prelimsearch_unit_test
phiblast_unit_test
split_query_unit_test
scoreblk_unit_test
hspstream_unit_test
rps_unit_test
gapinfo_unit_test
blasthits_unit_test
linkhsp_unit_test
blastengine_unit_test
subj_ranges_unit_test
version_reference_unit_test
ntscan_unit_test
aascan_unit_test
remote_blast_unit_test
uniform_search_unit_test
blastfilter_unit_test
blastoptions_unit_test
gencode_singleton_unit_test
bl2seq_unit_test
stat_unit_test
magicblast_unit_test
rmblast_blasthits_unit_test
rmblast_traceback_unit_test
)

