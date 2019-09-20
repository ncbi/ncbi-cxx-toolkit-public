#############################################################################
# $Id$
#############################################################################

set(SRC_BLAST_C_CORE
    ../core/aa_ungapped
    ../core/blast_aalookup
    ../core/blast_aascan
    ../core/blast_diagnostics
    ../core/blast_dynarray
    ../core/blast_encoding
    ../core/blast_engine
    ../core/blast_extend
    ../core/blast_filter
    ../core/blast_gapalign
    ../core/blast_hits
    ../core/blast_hspstream
    ../core/blast_hspstream_mt_utils
    ../core/blast_itree
    ../core/blast_kappa
    ../core/blast_lookup
    ../core/blast_message
    ../core/blast_nalookup
    ../core/blast_nascan
    ../core/blast_options
    ../core/blast_parameters
    ../core/blast_posit
    ../core/blast_program
    ../core/blast_psi
    ../core/blast_psi_priv
    ../core/blast_query_info
    ../core/blast_seg
    ../core/blast_seqsrc
    ../core/blast_setup
    ../core/blast_stat
    ../core/blast_sw
    ../core/blast_traceback
    ../core/blast_traceback_mt_priv
    ../core/blast_tune
    ../core/blast_util
    ../core/boost_erf
    ../core/gapinfo
    ../core/gencode_singleton
    ../core/greedy_align
    ../core/hspfilter_besthit
    ../core/hspfilter_collector
    ../core/hspfilter_culling
    ../core/hspfilter_mapper
    ../core/index_ungapped
    ../core/jumper
    ../core/link_hsps
    ../core/lookup_util
    ../core/lookup_wrap
    ../core/matrix_freq_ratios
    ../core/na_ungapped
    ../core/ncbi_math
    ../core/ncbi_std
    ../core/pattern
    ../core/phi_extend
    ../core/phi_gapalign
    ../core/phi_lookup
    ../core/split_query
    ../core/spliced_hits
)


NCBI_project_watchers(camacho madden)
NCBI_add_subdirectory(
  composition_adjustment core dbindex dbindex_search api proteinkmer
  format blastinput blast_sra_input igblast gumbel_params unit_tests
)

