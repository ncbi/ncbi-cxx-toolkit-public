# $Id$

APP = gencode_singleton_unit_test
SRC = gencode_singleton_unit_test

CPPFLAGS = -DNCBI_MODULE=BLAST $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)
LIB = test_boost $(BLAST_LIBS) xobjsimple $(OBJMGR_LIBS:ncbi_x%=ncbi_x%$(DLL))
LIBS = $(BLAST_THIRD_PARTY_LIBS) $(NETWORK_LIBS) $(CMPRS_LIBS) $(DL_LIBS) \
       $(ORIG_LIBS)
LDFLAGS = $(FAST_LDFLAGS)

CHECK_REQUIRES = MT
CHECK_CMD = gencode_singleton_unit_test
CHECK_COPY = gencode_singleton_unit_test.ini

WATCHERS = camacho
