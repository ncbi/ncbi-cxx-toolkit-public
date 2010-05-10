# $Id$

REQUIRES = unix

APP = test_reader_gicache
SRC = test_reader_gicache
LIB = $(OBJMGR_LIBS) $(GENBANK_READER_GICACHE_LIBS)

//CHECK_CMD = test_reader_gicache

WATCHERS = vasilche
