APP = prot_match
SRC = prot_match 

CPPFLAGS = $(ORIG_CPPFLAGS) 

LIB = protein_match xobjwrite $(OBJREAD_LIBS) xalnmgr xobjutil \
      gbseq entrez2cli entrez2 tables $(OBJMGR_LIBS)

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects -Cygwin


WATCHERS = foleyjp
