APP = hgvs_to_irep

SRC = hgvs_to_irep \
	  hgvs_lexer hgvs_parser semantic_actions \
	  hgvs_protein_parser hgvs_nucleic_acid_parser \

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE) 
LDFLAGS = $(ORIG_LDFLAGS)

LIB = varrep seq \
	  $(OBJREAD_LIBS) xregexp $(PCRE_LIB) xobjutil \
	  $(OBJMGR_LIBS) 

LIBS = $(PCRE_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES =
CHECK_COPY = 
CHECK_CMD = 


WATCHERS = foleyjp

