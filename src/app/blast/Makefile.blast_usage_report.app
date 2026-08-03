WATCHERS = fongah2 camacho

APP = blast_usage_report
SRC = blast_usage_report_app
LIB_ = $(BLAST_INPUT_LIBS) $(BLAST_LIBS) xregexp $(PCRE_LIB) $(OBJMGR_LIBS)
LIB = blast_app_util $(LIB_:%=%$(STATIC))

# De-universalize Mac builds to work around a PPC toolchain limitation
CFLAGS 	 = $(FAST_CXXFLAGS:ppc=i386) 
CXXFLAGS = $(FAST_CXXFLAGS:ppc=i386) 
LDFLAGS  = $(FAST_LDFLAGS:ppc=i386) 

CPPFLAGS = -DNCBI_MODULE=BLAST $(ORIG_CPPFLAGS) 
LIBS = $(BLAST_THIRD_PARTY_LIBS) $(GENBANK_THIRD_PARTY_LIBS) \
       $(NETWORK_LIBS) $(CMPRS_LIBS) $(ORIG_LIBS)

REQUIRES = objects -Cygwin

CHECK_CMD = blast_usage_report -privacy /CHECK_NAME=blast_usage_report_privacy
CHECK_CMD = blast_usage_report -status /CHECK_NAME=blast_usage_report_status
