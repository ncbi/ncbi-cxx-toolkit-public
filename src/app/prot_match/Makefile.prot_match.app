APP = prot_match
SRC = prot_match run_binary

CPPFLAGS = $(ORIG_CPPFLAGS) 

LIB = protein_match xobjwrite $(OBJREAD_LIBS) xalnmgr xobjutil \
      gbseq entrez2cli entrez2 tables $(DATA_LOADERS_UTIL_LIB) $(OBJMGR_LIBS)

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects -Cygwin


WATCHERS = foleyjp
