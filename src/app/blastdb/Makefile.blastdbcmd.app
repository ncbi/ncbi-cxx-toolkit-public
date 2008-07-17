APP = blastdbcmd
SRC = blastdbcmd blastdb_aux
REGEX_LIBS = xregexp $(PCRE_LIB)
LIB_ = xblastformat blastinput ncbi_xloader_blastdb $(REGEX_LIBS) $(BLAST_LIBS) \
	$(OBJMGR_LIBS)
LIB = $(LIB_:%=%$(STATIC))

CFLAGS   = $(FAST_CFLAGS)
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

CPPFLAGS = $(ORIG_CPPFLAGS)
LIBS = $(CMPRS_LIBS) $(DL_LIBS) $(PCRE_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

REQUIRES = objects -Cygwin
