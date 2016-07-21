# $Id$

WATCHERS = camacho madden fongah2

APP = magicblast
SRC = magicblast_app
LIB_ =$(BLAST_INPUT_LIBS) blast_sra_input $(BLAST_LIBS) $(SRAREAD_LIBS) $(OBJMGR_LIBS)
LIB = $(LIB_:%=%$(STATIC))

# De-universalize Mac builds to work around a PPC toolchain limitation
CFLAGS 	 = $(FAST_CXXFLAGS:ppc=i386) 
CXXFLAGS = $(FAST_CXXFLAGS:ppc=i386) 
LDFLAGS  = $(FAST_LDFLAGS:ppc=i386) 

CPPFLAGS = -DNCBI_MODULE=BLAST $(VDB_INCLUDE) $(ORIG_CPPFLAGS)
LIBS = $(VDB_STATIC_LIBS) $(CMPRS_LIBS) $(DL_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

REQUIRES = objects -Cygwin VDB

PROJ_TAG = gbench
