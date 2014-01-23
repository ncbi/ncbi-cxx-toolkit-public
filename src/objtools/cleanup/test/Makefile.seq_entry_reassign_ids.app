# $Id$

APP = seq_entry_reassign_ids
SRC = seq_entry_reassign_ids

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB = seqset $(SEQ_LIBS) pub medline biblio general xser xutil test_boost xncbi

LIBS = $(DL_LIBS) $(ORIG_LIBS)
LDFLAGS = $(FAST_LDFLAGS)

REQUIRES = objects

CHECK_CMD = seq_entry_reassign_ids
CHECK_COPY = test_cases

WATCHERS = vasilche
