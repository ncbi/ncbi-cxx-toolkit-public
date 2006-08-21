# $Id$

APP = writedb_unit_test
SRC = writedb_unit_test
LIB = xncbi seqdb writedb xobjutil blastdb $(OBJMGR_LIBS)

LIBS = $(CMPRS_LIBS) $(ORIG_LIBS)

REQUIRES = objects
CHECK_CMD = writedb_unit_test
CHECK_TIMEOUT = 60

