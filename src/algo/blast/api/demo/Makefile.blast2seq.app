APP = blast2seq
SRC = blast2seq blast_input
LIB = xblast xobjutil xobjread $(OBJMGR_LIBS) tables

CFLAGS   = $(FAST_CFLAGS)
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

LIBS = $(ORIG_LIBS) $(NETWORK_LIBS) $(DLL_LIBS)

REQUIRES = objects dbapi
