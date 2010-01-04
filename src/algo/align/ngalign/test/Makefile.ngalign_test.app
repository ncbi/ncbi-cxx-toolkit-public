# $Id$

ASN_DEP = seq

APP = ngalign_test
SRC = ngalign_test
LIB = xngalign  $(BLAST_LIBS) blastinput  xblast  ncbi_xloader_blastdb \
      ncbi_xloader_blastdb_rmt blast_services  xnetblastcli   \
      align_format   \
      xalgoalignnw xalgoseq xalnmgr xobjmgr xobjutil xobjread creaders \
      taxon1  $(GENBANK_LIBS)  $(QOBJMGR_ONLY_LIBS)  \
      xalgocontig_assembly xalgoalignutil xalnmgr xalgoalignnw  xblast\
      gpipe_objutil xqueryparse xutil \
      xregexp $(PCRE_LIB)


CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS = $(FAST_LDFLAGS)
LIBS = $(PCRE_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects

WATCHERS = boukn

