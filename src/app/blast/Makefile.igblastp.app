WATCHERS = camacho madden maning

APP = igblastp
SRC = igblastp_app
LIB_ = $(BLAST_INPUT_LIBS) $(BLAST_LIBS) $(OBJMGR_LIBS)
LIB = blast_app_util igblast $(LIB_:%=%$(STATIC))

# De-universalize Mac builds to work around a PPC toolchain limitation
CFLAGS   = $(FAST_CFLAGS:ppc=i386)
CXXFLAGS = $(FAST_CXXFLAGS:ppc=i386) -g
LDFLAGS  = $(FAST_LDFLAGS:ppc=i386) -g
 
CPPFLAGS = $(ORIG_CPPFLAGS) -g
LIBS = $(CMPRS_LIBS) $(DL_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

REQUIRES = objects -Cygwin
