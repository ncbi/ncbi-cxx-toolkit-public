APP = hgvs_to_seqfeat

SRC = hgvs_reader hgvs_lexer hgvs_parser hgvs_nucleic_acid_parser hgvs_protein_parser \
	  semantic_actions hgvs_validator id_resolver \
	  irep_to_seqfeat_common irep_to_seqfeat_errors na_irep_to_seqfeat protein_irep_to_seqfeat \
	  post_process variation_ref_utils


CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE) 
LDFLAGS = $(ORIG_LDFLAGS)

LIB = varrep seq xvalidate \
	  $(OBJREAD_LIBS) xregexp $(PCRE_LIB) xobjutil \
	  entrez2cli entrez2 $(OBJMGR_LIBS)  


LIBS = $(PCRE_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES =
CHECK_COPY = 
CHECK_CMD = 


WATCHERS = foleyjp

