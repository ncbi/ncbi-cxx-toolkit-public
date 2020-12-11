# $Id$

NCBI_begin_lib(snp SHARED)
  NCBI_hosts_projects(
    gui/packages/pkg_snp/utils/w_snp_utils
    gui/packages/pkg_snp/filter/w_snp_filter
    gui/packages/pkg_snp/track/w_snp_track
    gui/packages/pkg_snp/hapmap/w_hapmap
    gui/packages/pkg_snp/ld/w_snp_ld
    gui/packages/pkg_snp/bins/w_snp_bins
  )
  NCBI_add_definitions(NCBI_GUIWIDGETS_SNP_EXPORTS)
NCBI_end_lib()

NCBI_begin_lib(pkg_snp SHARED)
  NCBI_hosts_projects(
    gui/packages/pkg_snp/package/pkg_snp
  )
NCBI_end_lib()
