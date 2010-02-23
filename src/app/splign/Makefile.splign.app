#################################
# $Id$
#################################

APP = splign
SRC = splign_app


BLAST_DB_DATA_LOADER_LIBS = ncbi_xloader_blastdb ncbi_xloader_blastdb_rmt
LIB = xalgoalignsplign xalgoalignutil xqueryparse xalgoalignnw xalgoseq \
      $(BLAST_DB_DATA_LOADER_LIBS) \
	  ncbi_xloader_lds lds bdb \
      xalnmgr xregexp $(PCRE_LIB) taxon1 \
      $(BLAST_LIBS:%=%$(STATIC)) \
      $(OBJMGR_LIBS:%=%$(STATIC))

LIBS = $(BERKELEYDB_STATIC_LIBS) \
       $(PCRE_LIBS) \
       $(CMPRS_LIBS) \
       $(NETWORK_LIBS) \
       $(DL_LIBS) \
       $(ORIG_LIBS)


# Comment out -D ALGOALIGN_NW_SPLIGN_MAKE_PUBLIC_BINARY to:
#   1) Use FASTA-style sequence IDs in the output.
#   2) Print individual exon scores in the last column of the output.
#   3) Use 'sense' as the default transcript orientation regardless of -type.

CXXFLAGS = $(FAST_CXXFLAGS) -D ALGOALIGN_NW_SPLIGN_MAKE_PUBLIC_BINARY

LDFLAGS  = $(FAST_LDFLAGS)

REQUIRES = algo BerkeleyDB objects -Cygwin
