APP = blastdbcheck
SRC = blastdbcheck blastdb_aux
REGEX_LIBS = xregexp $(PCRE_LIB)
LIB_ = blastinput ncbi_xloader_blastdb $(BLAST_LIBS) $(REGEX_LIBS) \
	$(OBJMGR_LIBS)
LIB = $(LIB_:%=%$(STATIC))

CFLAGS   = $(FAST_CFLAGS)
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

CPPFLAGS = $(ORIG_CPPFLAGS)
LIBS = $(CMPRS_LIBS) $(DL_LIBS) $(PCRE_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

REQUIRES = objects -Cygwin
REGEX_LIBS = xregexp 
