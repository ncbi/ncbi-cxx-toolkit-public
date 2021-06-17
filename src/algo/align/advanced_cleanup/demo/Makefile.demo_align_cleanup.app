# $Id$
APP  = demo_align_cleanup
SRC  = demo_align_cleanup

LIB  = xaligncleanup prosplign xalgoalignsplign xalgoalignnw xalgoalignutil \
       xalgoseq xqueryparse $(DATA_LOADERS_UTIL_LIB) $(SRAREAD_LIBS) \
       $(BLAST_LIBS) taxon1 xregexp $(PCRE_LIB) $(OBJMGR_LIBS)

LIBS = $(CMPRS_LIBS) $(DATA_LOADERS_UTIL_LIBS) $(BLAST_THIRD_PARTY_LIBS) \
       $(PCRE_LIBS) $(GENBANK_THIRD_PARTY_LIBS) $(ORIG_LIBS)
