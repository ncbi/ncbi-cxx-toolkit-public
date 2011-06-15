WATCHERS = camacho madden maning fongah2

APP = rpstblastn
SRC = rpstblastn_app
LIB_ = $(BLAST_INPUT_LIBS) $(BLAST_LIBS) $(OBJMGR_LIBS)
LIB = blast_app_util $(LIB_:%=%$(STATIC))

# De-universalize Mac builds to work around a PPC toolchain limitation
CFLAGS   = $(FAST_CFLAGS:ppc=i386)
CXXFLAGS = $(FAST_CXXFLAGS:ppc=i386)
LDFLAGS  = $(FAST_LDFLAGS:ppc=i386)

CPPFLAGS = $(ORIG_CPPFLAGS)
LIBS = $(CMPRS_LIBS) $(DL_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

REQUIRES = objects -Cygwin
