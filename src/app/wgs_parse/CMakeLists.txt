#############################################################################
# $Id$
#############################################################################

NCBI_begin_app(wgs_parse)
  NCBI_sources(
    wgs_asn
    wgs_errors
    wgs_filelist
    wgs_id1
    wgs_master
    wgs_med
    wgs_params
    wgs_parse
    wgs_pubs
    wgs_sub
    wgs_tax
    wgs_text_accession
    wgs_utils
  )
  NCBI_requires(FreeTDS)
  NCBI_uses_toolkit_libraries(
    ncbi_xreader_pubseqos ncbi_xreader_pubseqos2 ncbi_xloader_genbank
    xcleanup id1cli taxon1 fix_pub)
NCBI_end_app()
