APP = blast2seq
SRC = blast2seq
LIB = xblast xobjutil xobjread $(OBJMGR_LIBS)

CPPFLAGS = $(ORIG_CPPFLAGS) $(FAST_CPPFLAGS) -I$(top_srcdir)/include/algo/blast/core
CFLAGS   = $(ORIG_CFLAGS)   $(FAST_CFLAGS) -I$(top_srcdir)/include/algo/blast/core
CXXFLAGS = $(ORIG_CXXFLAGS) $(FAST_CXXFLAGS) -I$(top_srcdir)/include/algo/blast/core
LDFLAGS  = $(ORIG_LDFLAGS)  $(FAST_LDFLAGS)

REQUIRES = objects
